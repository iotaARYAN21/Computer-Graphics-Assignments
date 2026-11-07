#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"
#include "../common/Mesh.h"
#include "../common/Starfield.h"
#include "../models/Cylinder.h"
#include "../models/Cube.h"
#include "../models/Fairing.h"
#include "../models/Plane.h"
#include "Flight.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>

using namespace std;

const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 900;
const float PI = 3.14159265359f;

// ---------------------------------------------------------------------------
// Scene scale
// ---------------------------------------------------------------------------
// Earth is drawn with radius 30. At true scale the 44 m tall PSLV would be
// invisible, so the vehicle (and everything near it) is drawn about 140x
// larger than real. The trajectory itself uses real kilometres from the
// flight model, so the vehicle still climbs several of its own heights
// before the first separation.
const float EARTH_RADIUS = 30.0f;
const float KM_TO_WORLD = EARTH_RADIUS / static_cast<float>(EARTH_RADIUS_KM);
const float ROCKET_SCALE = 0.003f;               // model units -> world units
const float MODEL_UNITS_PER_METRE = 9.83f / 44.4f; // model height / real height
const float PAD_TOP = 0.41f;                     // pad height in model units

// Separated parts are drawn relative to the vehicle with the same
// magnification as the vehicle, so separations look right next to it.
const float SEPARATION_TO_WORLD = 1000.0f * MODEL_UNITS_PER_METRE * ROCKET_SCALE;
const float DEBRIS_HIDE_DISTANCE = 150.0f; // model units

// ---------------------------------------------------------------------------
// Mission data: PSLV-C25 / Mars Orbiter Mission (Mangalyaan)
// Launch: 5 November 2013, 09:08 UTC (14:38 IST), First Launch Pad,
// Satish Dhawan Space Centre, Sriharikota.
// ---------------------------------------------------------------------------
const float SITE_LATITUDE = 13.72f;
const float SITE_LONGITUDE = 80.23f;
const double LAUNCH_AZIMUTH = 104.0; // degrees from north, over the Bay of Bengal

// Directions of the Sun and Mars at launch time, written as the point on
// Earth where each one is overhead (latitude = declination, longitude =
// right ascension - Greenwich sidereal time). Computed from the JPL
// approximate planetary elements for 2013-11-05 09:08 UTC:
//   Sun : RA 220.5 deg, Dec -15.7 deg -> Sun in the south-west at
//         Sriharikota, about 40 deg high (afternoon)
//   Mars: RA 164.1 deg, Dec  +8.5 deg, 1.89 AU from Earth (in Leo, a
//         morning planet, below the horizon at launch time)
const float SUN_LATITUDE = -15.72f;
const float SUN_LONGITUDE = 38.71f;
const float MARS_LATITUDE = 8.51f;
const float MARS_LONGITUDE = -17.69f;

// Mars and the Sun are drawn in the correct directions but far closer
// than real (true distances would be ~1e6 Earth radii). Sizes: Mars has
// its real size relative to Earth (0.53 R_earth); the Sun is shrunk to a
// small disc.
const float MARS_DISTANCE = 800.0f;
const float MARS_RADIUS = 0.532f * EARTH_RADIUS;
const float SUN_DISTANCE = 1500.0f;
const float SUN_RADIUS = 20.0f;
const float MARS_ROTATION_DEG_PER_S = 360.0f / 88643.0f; // sidereal sol

const float EARTH_ROTATION_DEG_PER_S = 360.0f / 86164.0f; // sidereal day

// ---------------------------------------------------------------------------
// Cameras
// ---------------------------------------------------------------------------
struct OrbitCamera
{
    float yaw, pitch, distance, minDistance, maxDistance, minPitch;
};

// 0 = Earth view (world units), 1 = launch site, 2 = vehicle (model units),
// 3 = planets view (world units)
const int CAMERA_COUNT = 4;
OrbitCamera cameras[CAMERA_COUNT] = {
    {0.0f, 0.0f, 90.0f, 33.0f, 650.0f, -89.0f},
    {-150.0f, 8.0f, 32.0f, 8.0f, 2000.0f, 2.0f}, // sunlit (south-west) side
    {-150.0f, 10.0f, 22.0f, 5.0f, 300.0f, -80.0f},
    {25.0f, 0.0f, 165.0f, 60.0f, 700.0f, -80.0f}, // behind Earth, Mars beyond
};
const char* CAMERA_NAMES[CAMERA_COUNT] = {"Earth view", "Launch site", "Vehicle", "Planets (Earth + Mars)"};
int cameraMode = 1;

const int WARP_LEVELS[] = {1, 2, 5, 10, 20, 50};
const int WARP_COUNT = 6;
int warpIndex = 3; // 10x

Flight flight;

// Flight path drawn as a line in the Earth view (positions in the
// Earth-fixed frame, world units).
vector<glm::vec3> trail;
double lastTrailTime = -1.0;

vector<float> earthVertices;
vector<unsigned int> earthIndices;

string readFile(const string& path) {
    ifstream file(path);
    if (!file) throw runtime_error("Cannot open shader file: " + path);
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compileShader(GLenum type, const string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        cerr << "Shader compilation error:\n" << log << '\n';
        glDeleteShader(shader);
        throw runtime_error("Shader compilation failed");
    }
    return shader;
}

GLuint createProgram(const string& vertexPath, const string& fragmentPath) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, readFile(vertexPath));
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, readFile(fragmentPath));
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// Returns 0 if the image cannot be loaded (used for optional textures).
GLuint loadTexture(const char* path, bool repeat) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* image = stbi_load(path, &width, &height, &channels, 0);
    if (!image) return 0;

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    stbi_image_free(image);
    return texture;
}

void generateSphere(float radius, int stacks, int sectors) {
    earthVertices.clear();
    earthIndices.clear();
    for (int i = 0; i <= stacks; ++i) {
        float theta = PI * static_cast<float>(i) / stacks;
        for (int j = 0; j <= sectors; ++j) {
            float phi = 2.0f * PI * static_cast<float>(j) / sectors;
            float x = radius * sin(theta) * cos(phi);
            float y = radius * cos(theta);
            float z = radius * sin(theta) * sin(phi);
            earthVertices.insert(earthVertices.end(), {
                x, y, z,
                x/radius, y/radius, z/radius,
                1.0f - static_cast<float>(j)/sectors, 1.0f - static_cast<float>(i)/stacks
            });
        }
    }
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            unsigned int first = i * (sectors + 1) + j;
            unsigned int second = first + sectors + 1;
            earthIndices.insert(earthIndices.end(), {first, second, first + 1, second, second + 1, first + 1});
        }
    }
}

void setMatrices(GLuint program, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection) {
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
}

// Unit vector from Earth's centre to a latitude/longitude, matching the
// texture mapping of generateSphere(): u = 1 - phi / 2pi and the
// equirectangular texture has longitude -180 at u = 0, so phi = 180 - lon.
glm::vec3 surfaceDirection(float lat, float lon) {
    float latRad = glm::radians(lat);
    float phi = glm::radians(180.0f - lon);
    return glm::vec3(cos(latRad) * cos(phi), sin(latRad), cos(latRad) * sin(phi));
}

// Local frame on the surface: x = east, y = up, z = north.
glm::mat4 getSurfaceTransform(float lat, float lon, float radius) {
    glm::vec3 up = surfaceDirection(lat, lon);
    glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), up));
    glm::vec3 forward = glm::normalize(glm::cross(up, right));

    glm::mat4 transform(1.0f);
    transform[0] = glm::vec4(right, 0.0f);
    transform[1] = glm::vec4(up, 0.0f);
    transform[2] = glm::vec4(forward, 0.0f);
    transform[3] = glm::vec4(up * radius, 1.0f);
    return transform;
}

float smoothStep(float edge0, float edge1, float x) {
    float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void resetMission() {
    flight.reset(glm::dvec3(surfaceDirection(SITE_LATITUDE, SITE_LONGITUDE)), LAUNCH_AZIMUTH);
    trail.clear();
    lastTrailTime = -1.0;

    // Point the Earth-view camera at India.
    glm::vec3 d = surfaceDirection(SITE_LATITUDE, SITE_LONGITUDE);
    cameras[0].pitch = glm::degrees(asin(d.y)) + 10.0f;
    cameras[0].yaw = glm::degrees(atan2(d.x, d.z));
}

void processInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    // Camera orbit and zoom for the active camera.
    OrbitCamera& cam = cameras[cameraMode];
    const float angleSpeed = 60.0f * deltaTime;
    const float zoomFactor = 1.0f + 1.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) cam.yaw -= angleSpeed;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) cam.yaw += angleSpeed;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) cam.pitch = glm::min(cam.pitch + angleSpeed, 89.0f);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) cam.pitch = glm::max(cam.pitch - angleSpeed, cam.minPitch);
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) cam.distance = glm::max(cam.distance / zoomFactor, cam.minDistance);
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) cam.distance = glm::min(cam.distance * zoomFactor, cam.maxDistance);

    // Edge-triggered keys.
    static bool wasDown[GLFW_KEY_LAST + 1] = {false};
    auto pressed = [&](int key) {
        bool down = glfwGetKey(window, key) == GLFW_PRESS;
        bool result = down && !wasDown[key];
        wasDown[key] = down;
        return result;
    };

    if (pressed(GLFW_KEY_C)) cameraMode = (cameraMode + 1) % CAMERA_COUNT;
    if (pressed(GLFW_KEY_L)) flight.launch();
    if (pressed(GLFW_KEY_R)) resetMission();
    if (pressed(GLFW_KEY_EQUAL) || pressed(GLFW_KEY_KP_ADD)) warpIndex = glm::min(warpIndex + 1, WARP_COUNT - 1);
    if (pressed(GLFW_KEY_MINUS) || pressed(GLFW_KEY_KP_SUBTRACT)) warpIndex = glm::max(warpIndex - 1, 0);
}

const char* phaseName() {
    if (!flight.launched) return "on pad";
    switch (flight.phase) {
        case PHASE_PS1_STRAPONS: return "PS1 + strap-ons";
        case PHASE_PS1: return "PS1";
        case PHASE_PS2: return "PS2";
        case PHASE_PS3: return "PS3";
        case PHASE_PS4: return "PS4";
        default: return flight.satelliteReleased ? "Mangalyaan released" : "coast";
    }
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "a2-scene - PSLV-C25 / Mangalyaan", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;
    glGetError();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    try {
        // Fine tessellation so the flat facets stay close to the true sphere
        // under the (small) launch pad.
        generateSphere(EARTH_RADIUS, 256, 512);
        auto earthMesh = make_unique<Mesh>(earthVertices, earthIndices);
        auto stars = make_unique<Starfield>(4000, 100.0f);

        // Unit sphere shared by Mars and the Sun.
        generateSphere(1.0f, 64, 128);
        auto unitSphere = make_unique<Mesh>(earthVertices, earthIndices);

        Cylinder cylinder(64, false);
        Cylinder cone(64, true);
        Cube cube;
        Plane ground;
        Fairing fairingA(0, 36);
        Fairing fairingB(1, 36);

        GLuint earthProgram = createProgram("shaders/vertex.glsl", "shaders/fragment.glsl");
        GLuint atmosphereProgram = createProgram("shaders/atmosphere_vertex.glsl", "shaders/atmosphere_fragment.glsl");
        GLuint rocketProgram = createProgram("shaders/cube_vertex.glsl", "shaders/cube_fragment.glsl");
        GLuint groundProgram = createProgram("shaders/ground_vertex.glsl", "shaders/ground_fragment.glsl");
        GLuint starProgram = createProgram("shaders/star_vertex.glsl", "shaders/star_fragment.glsl");

        // Dynamic line buffer for the flight-path trail.
        GLuint trailVAO, trailVBO;
        glGenVertexArrays(1, &trailVAO);
        glGenBuffers(1, &trailVBO);
        glBindVertexArray(trailVAO);
        glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);

        GLuint earthTex = loadTexture("textures/earth.jpg", false);
        if (!earthTex) throw runtime_error("Cannot load texture: textures/earth.jpg");
        GLuint marsTex = loadTexture("textures/Mars.jpg", false);
        if (!marsTex) throw runtime_error("Cannot load texture: textures/Mars.jpg");
        GLuint groundTex = loadTexture("textures/ground.jpg", true);
        if (!groundTex) cerr << "Note: textures/ground.jpg not found, using a plain ground colour.\n";

        resetMission();

        cout << "\n=== a2-scene: PSLV-C25 / Mangalyaan ==="
             << "\n  L        : lift-off"
             << "\n  C        : switch camera (Earth view / Launch site / Vehicle / Planets)"
             << "\n  Arrows   : orbit the current camera"
             << "\n  [ and ]  : zoom in / out"
             << "\n  + and -  : time warp (1x to 50x)"
             << "\n  R        : reset to the pad"
             << "\n  Esc      : exit"
             << "\n  Stage and fairing separations happen automatically at their altitudes."
             << "\n========================================\n\n";

        double lastFrame = glfwGetTime();
        double lastTitleUpdate = 0.0;

        while (!glfwWindowShouldClose(window)) {
            double currentFrame = glfwGetTime();
            float deltaTime = static_cast<float>(currentFrame - lastFrame);
            lastFrame = currentFrame;
            processInput(window, deltaTime);

            // ---------------- Simulation ----------------
            // Briefly drop to real time after each event so separations can
            // be seen, otherwise use the selected time warp.
            int warp = WARP_LEVELS[warpIndex];
            if (flight.missionTime - flight.lastEventTime < 6.0) warp = 1;
            flight.update(deltaTime * warp);
            for (const string& msg : flight.messages) cout << msg << endl;
            flight.messages.clear();

            float altitude = static_cast<float>(altitudeKm(flight.vehicle.pos));

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            if (height == 0) height = 1;
            glViewport(0, 0, width, height);

            // ---------------- World transforms ----------------
            // Earth turns at its real rate (times the warp) during the mission;
            // at T+0 the Earth-fixed frame equals the world frame.
            float earthRot = EARTH_ROTATION_DEG_PER_S * static_cast<float>(flight.missionTime);
            glm::mat4 earthModel = glm::rotate(glm::mat4(1.0f), glm::radians(earthRot), glm::vec3(0, 1, 0));

            glm::mat4 worldSiteTransform = earthModel * getSurfaceTransform(SITE_LATITUDE, SITE_LONGITUDE, EARTH_RADIUS);
            glm::vec3 sitePos = glm::vec3(worldSiteTransform[3]);
            glm::vec3 siteRight = glm::vec3(worldSiteTransform[0]);
            glm::vec3 siteUp = glm::vec3(worldSiteTransform[1]);
            glm::vec3 siteForward = glm::vec3(worldSiteTransform[2]);

            // Vehicle position (Earth-fixed, world units). The pad height is a
            // constant offset so the rocket starts standing on the pad.
            glm::vec3 siteUpFixed = surfaceDirection(SITE_LATITUDE, SITE_LONGITUDE);
            glm::vec3 vehiclePosFixed = glm::vec3(flight.vehicle.pos) * KM_TO_WORLD + siteUpFixed * (PAD_TOP * ROCKET_SCALE);

            auto placeFrame = [&](const glm::vec3& posFixed, const glm::mat3& frame) {
                glm::mat4 m(frame);
                m[3] = glm::vec4(posFixed, 1.0f);
                return earthModel * m * glm::scale(glm::mat4(1.0f), glm::vec3(ROCKET_SCALE));
            };
            // Separated bodies: offset from the vehicle, magnified like the vehicle.
            auto separatedPos = [&](const Body& body) {
                glm::vec3 offset = glm::vec3(body.pos - flight.vehicle.pos);
                return vehiclePosFixed + offset * SEPARATION_TO_WORLD;
            };

            glm::mat4 rocketTransform = placeFrame(vehiclePosFixed, flight.vehicleFrame());
            glm::vec3 rocketRight = glm::normalize(glm::vec3(rocketTransform[0]));
            glm::vec3 rocketUp = glm::normalize(glm::vec3(rocketTransform[1]));
            glm::vec3 rocketForward = glm::normalize(glm::vec3(rocketTransform[2]));
            // Middle of whatever is left of the stack, used as camera target.
            const float STACK_CENTRE[] = {4.9f, 4.9f, 6.2f, 7.5f, 8.2f, 8.2f};
            glm::vec3 rocketCentre = glm::vec3(rocketTransform * glm::vec4(0.0f, STACK_CENTRE[flight.phase], 0.0f, 1.0f));

            glm::mat4 satelliteBase = rocketTransform;
            if (flight.satelliteReleased)
                satelliteBase = placeFrame(separatedPos(flight.satellite), flight.satelliteFrame);
            const glm::vec3 localSatPos(0.0f, 8.75f, 0.0f);
            glm::vec3 worldSatPos = glm::vec3(satelliteBase * glm::vec4(localSatPos, 1.0f));

            // ---------------- Cameras ----------------
            OrbitCamera& cam = cameras[cameraMode];
            float cy = glm::radians(cam.yaw);
            float cp = glm::radians(cam.pitch);
            glm::vec3 orbit(cos(cp) * sin(cy), sin(cp), cos(cp) * cos(cy));

            glm::mat4 view;
            glm::vec3 cameraPos;
            float nearPlane = 0.0005f;

            if (cameraMode == 0) {
                // Earth view: distant camera around Earth's centre.
                cameraPos = orbit * cam.distance;
                view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                nearPlane = 0.5f;
            } else if (cameraMode == 1) {
                // Launch-site camera: fixed near the pad, tracks the vehicle.
                float d = cam.distance * ROCKET_SCALE;
                glm::vec3 offset = siteRight * (d * orbit.x) + siteUp * (d * orbit.y) + siteForward * (d * orbit.z);
                cameraPos = sitePos + offset;
                glm::vec3 target = flight.satelliteReleased ? worldSatPos : rocketCentre;
                view = glm::lookAt(cameraPos, target, siteUp);
            } else if (cameraMode == 3) {
                // Planets view: from just behind Earth (on the side away from
                // Mars), looking out towards Mars so both are in the frame.
                // The orbit angles are measured around that line.
                glm::vec3 towardMars = surfaceDirection(MARS_LATITUDE, MARS_LONGITUDE);
                glm::vec3 right = glm::normalize(glm::cross(towardMars, glm::vec3(0.0f, 1.0f, 0.0f)));
                glm::vec3 up = glm::cross(right, towardMars);
                cameraPos = (right * orbit.x + up * orbit.y - towardMars * orbit.z) * cam.distance;
                view = glm::lookAt(cameraPos, towardMars * (0.3f * MARS_DISTANCE), up);
                nearPlane = 0.5f;
            } else {
                // Vehicle camera: moves with the launch vehicle (then the payload).
                glm::vec3 target = flight.satelliteReleased ? worldSatPos : rocketCentre;
                float d = cam.distance * ROCKET_SCALE;
                glm::vec3 offset = rocketRight * (d * orbit.x) + rocketUp * (d * orbit.y) + rocketForward * (d * orbit.z);
                cameraPos = target + offset;
                view = glm::lookAt(cameraPos, target, rocketUp);
            }
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / height, nearPlane, 2000.0f);

            // ---------------- Lighting and sky ----------------
            // The Sun is fixed in space; Earth rotates underneath it.
            glm::vec3 sunDir = surfaceDirection(SUN_LATITUDE, SUN_LONGITUDE);
            glm::vec3 lightPos = sunDir * 100000.0f;

            // Daytime blue sky when the camera is low in the sunlit
            // atmosphere, fading to black space with altitude.
            float cameraAltKm = (glm::length(cameraPos) - EARTH_RADIUS) / KM_TO_WORLD;
            float daylight = smoothStep(-0.05f, 0.2f, glm::dot(glm::normalize(cameraPos), sunDir));
            float skyAmount = (1.0f - smoothStep(0.0f, 60.0f, cameraAltKm)) * daylight;
            glm::vec3 skyColour = glm::mix(glm::vec3(0.0f, 0.0f, 0.01f), glm::vec3(0.42f, 0.62f, 0.88f), skyAmount);
            glClearColor(skyColour.r, skyColour.g, skyColour.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // ---------------- Stars ----------------
            glDisable(GL_DEPTH_TEST);
            setMatrices(starProgram, glm::mat4(1.0f), view, projection);
            glUniform1f(glGetUniformLocation(starProgram, "starVisibility"), 1.0f - skyAmount);
            stars->draw();
            glEnable(GL_DEPTH_TEST);

            // ---------------- Sun and Mars ----------------
            // Drawn as a background layer with their own near/far range,
            // then the depth buffer is cleared, so the close-up cameras
            // (very small near plane) keep good depth precision for Earth
            // and the vehicle. Earth drawn afterwards still hides them.
            {
                glm::mat4 farProjection = glm::perspective(glm::radians(45.0f), (float)width / height, 1.0f, 5000.0f);

                // Sun: self-lit disc in the light direction.
                glm::mat4 sunModel = glm::translate(glm::mat4(1.0f), sunDir * SUN_DISTANCE);
                sunModel = glm::scale(sunModel, glm::vec3(SUN_RADIUS));
                setMatrices(rocketProgram, sunModel, view, farProjection);
                glUniform3f(glGetUniformLocation(rocketProgram, "objectColor"), 1.0f, 0.93f, 0.75f);
                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 1.0f);
                unitSphere->draw();
                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 0.0f);

                // Mars: textured, lit by the Sun, slowly rotating.
                glm::vec3 marsDir = surfaceDirection(MARS_LATITUDE, MARS_LONGITUDE);
                float marsRot = MARS_ROTATION_DEG_PER_S * static_cast<float>(flight.missionTime);
                glm::mat4 marsModel = glm::translate(glm::mat4(1.0f), marsDir * MARS_DISTANCE);
                marsModel = glm::rotate(marsModel, glm::radians(marsRot), glm::vec3(0, 1, 0));
                marsModel = glm::scale(marsModel, glm::vec3(MARS_RADIUS));
                setMatrices(earthProgram, marsModel, view, farProjection);
                glUniform3fv(glGetUniformLocation(earthProgram, "lightPos"), 1, glm::value_ptr(lightPos));
                glUniform1i(glGetUniformLocation(earthProgram, "earthTexture"), 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, marsTex);
                unitSphere->draw();
                glBindTexture(GL_TEXTURE_2D, 0);

                glClear(GL_DEPTH_BUFFER_BIT);
            }

            // ---------------- Atmosphere ----------------
            glDepthMask(GL_FALSE);
            setMatrices(atmosphereProgram, glm::scale(earthModel, glm::vec3(1.03f)), view, projection);
            glUniform3fv(glGetUniformLocation(atmosphereProgram, "cameraPos"), 1, glm::value_ptr(cameraPos));
            earthMesh->draw();
            glDepthMask(GL_TRUE);

            // ---------------- Earth ----------------
            setMatrices(earthProgram, earthModel, view, projection);
            glUniform3fv(glGetUniformLocation(earthProgram, "lightPos"), 1, glm::value_ptr(lightPos));
            glUniform1i(glGetUniformLocation(earthProgram, "earthTexture"), 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, earthTex);
            earthMesh->draw();

            // ---------------- Launch site ----------------
            glm::mat4 siteModel = worldSiteTransform * glm::scale(glm::mat4(1.0f), glm::vec3(ROCKET_SCALE));
            if (cameraMode == 1 || cameraMode == 2) {
                // Ground patch around the pad (hidden in the distant view).
                glm::mat4 groundModel = glm::translate(siteModel, glm::vec3(0.0f, 0.05f, 0.0f));
                groundModel = glm::scale(groundModel, glm::vec3(5.0f, 1.0f, 5.0f));
                setMatrices(groundProgram, groundModel, view, projection);
                glUniform3fv(glGetUniformLocation(groundProgram, "lightPos"), 1, glm::value_ptr(lightPos));
                glUniform1i(glGetUniformLocation(groundProgram, "useTexture"), groundTex != 0);
                glUniform1i(glGetUniformLocation(groundProgram, "groundTexture"), 0);
                glUniform1f(glGetUniformLocation(groundProgram, "uvRepeat"), 20.0f);
                glBindTexture(GL_TEXTURE_2D, groundTex);
                ground.draw();
            }
            glBindTexture(GL_TEXTURE_2D, 0);

            glm::mat4 padBase = glm::translate(siteModel, glm::vec3(0.0f, 0.23f, 0.0f));
            padBase = glm::scale(padBase, glm::vec3(6.0f, 0.36f, 5.0f));
            setMatrices(rocketProgram, padBase, view, projection);
            glUniform3fv(glGetUniformLocation(rocketProgram, "lightPos"), 1, glm::value_ptr(lightPos));
            glUniform3f(glGetUniformLocation(rocketProgram, "objectColor"), 0.48f, 0.49f, 0.52f);
            cube.draw();

            // ---------------- Vehicle ----------------
            auto drawComponent = [&](auto& drawable, const glm::mat4& base, const glm::vec3& localPos, const glm::vec3& scale, const glm::vec3& color, glm::mat4 customRot = glm::mat4(1.0f)) {
                glm::mat4 model = glm::translate(base, localPos);
                model *= customRot;
                model = glm::scale(model, scale);
                setMatrices(rocketProgram, model, view, projection);
                glUniform3fv(glGetUniformLocation(rocketProgram, "lightPos"), 1, glm::value_ptr(lightPos));
                glUniform3fv(glGetUniformLocation(rocketProgram, "objectColor"), 1, glm::value_ptr(color));
                drawable.draw();
            };

            const glm::vec3 white(0.88f, 0.88f, 0.84f);
            const glm::vec3 orange(0.90f, 0.30f, 0.08f);
            const glm::vec3 dark(0.22f, 0.24f, 0.28f);
            const glm::vec3 gold(0.82f, 0.63f, 0.18f);

            auto drawBooster = [&](const glm::mat4& base, int i, glm::mat4 rot) {
                float a = glm::radians(45.0f + 90.0f * i);
                glm::vec3 pos(0.92f * cos(a), 1.85f, 0.92f * sin(a));
                drawComponent(cylinder, base, pos, glm::vec3(0.38f, 3.1f, 0.38f), white, rot);
                glm::vec3 tip = glm::vec3(rot * glm::vec4(0.0f, 1.65f, 0.0f, 0.0f));
                drawComponent(cone, base, pos + tip, glm::vec3(0.38f, 0.48f, 0.38f), orange, rot);
            };
            auto drawStage1 = [&](const glm::mat4& base) {
                drawComponent(cylinder, base, glm::vec3(0.0f, 1.75f, 0.0f), glm::vec3(1.45f, 3.5f, 1.45f), white);
                drawComponent(cylinder, base, glm::vec3(0.0f, 3.58f, 0.0f), glm::vec3(1.48f, 0.18f, 1.48f), dark);
            };
            auto drawStage2 = [&](const glm::mat4& base) {
                drawComponent(cylinder, base, glm::vec3(0.0f, 4.65f, 0.0f), glm::vec3(1.12f, 2.0f, 1.12f), orange);
                drawComponent(cylinder, base, glm::vec3(0.0f, 5.72f, 0.0f), glm::vec3(1.16f, 0.16f, 1.16f), dark);
            };
            auto drawStage3 = [&](const glm::mat4& base) {
                drawComponent(cylinder, base, glm::vec3(0.0f, 6.65f, 0.0f), glm::vec3(0.88f, 1.75f, 0.88f), white);
            };
            const glm::vec3 fairingScale(1.22f, 1.55f, 1.22f);
            const glm::vec3 fairingBasePos(0.0f, 9.05f, 0.0f);

            Phase phase = flight.phase;
            if (phase == PHASE_PS1_STRAPONS)
                for (int i = 0; i < 4; ++i) drawBooster(rocketTransform, i, glm::mat4(1.0f));
            if (phase <= PHASE_PS1) drawStage1(rocketTransform);
            if (phase <= PHASE_PS2) drawStage2(rocketTransform);
            if (phase <= PHASE_PS3) drawStage3(rocketTransform);

            // PS4 and payload adapter stay with the vehicle.
            drawComponent(cylinder, rocketTransform, glm::vec3(0.0f, 7.78f, 0.0f), glm::vec3(0.72f, 0.55f, 0.72f), dark);
            drawComponent(cylinder, rocketTransform, glm::vec3(0.0f, 8.18f, 0.0f), glm::vec3(0.62f, 0.28f, 0.62f), gold);

            if (!flight.fairingSeparated) {
                drawComponent(fairingA, rocketTransform, fairingBasePos, fairingScale, white);
                drawComponent(fairingB, rocketTransform, fairingBasePos, fairingScale, white);
            }

            // ---------------- Separated parts ----------------
            for (const Debris& d : flight.debris) {
                if (!d.active) continue;
                glm::vec3 posFixed = separatedPos(d.body);
                if (glm::length(posFixed - vehiclePosFixed) > DEBRIS_HIDE_DISTANCE * ROCKET_SCALE) continue;

                glm::mat4 base = placeFrame(posFixed, d.frame);
                float t = static_cast<float>(flight.missionTime - d.releaseTime);

                switch (d.kind) {
                    case DEBRIS_BOOSTER: {
                        float a = glm::radians(45.0f + 90.0f * d.index);
                        glm::vec3 outDir(cos(a), 0.0f, sin(a));
                        glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(12.0f * t), glm::cross(glm::vec3(0, 1, 0), outDir));
                        drawBooster(base, d.index, rot);
                        break;
                    }
                    case DEBRIS_STAGE1: drawStage1(base); break;
                    case DEBRIS_STAGE2: drawStage2(base); break;
                    case DEBRIS_STAGE3: drawStage3(base); break;
                    case DEBRIS_FAIRING_A:
                    case DEBRIS_FAIRING_B: {
                        // Each half swings open about its base and drifts away.
                        float sign = (d.kind == DEBRIS_FAIRING_A) ? 1.0f : -1.0f;
                        glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(sign * 25.0f * t), glm::vec3(1, 0, 0));
                        glm::mat4 hinge = glm::translate(base, fairingBasePos - glm::vec3(0.0f, 0.5f * fairingScale.y, 0.0f));
                        hinge *= rot;
                        glm::vec3 up(0.0f, 0.5f * fairingScale.y, 0.0f);
                        if (d.kind == DEBRIS_FAIRING_A) drawComponent(fairingA, hinge, up, fairingScale, white);
                        else drawComponent(fairingB, hinge, up, fairingScale, white);
                        break;
                    }
                }
            }

            // ---------------- Mangalyaan ----------------
            if (flight.fairingSeparated) {
                float dProg = 0.0f;
                if (flight.satelliteReleased)
                    dProg = glm::clamp(static_cast<float>(flight.missionTime - flight.satelliteReleaseTime) / 3.0f, 0.0f, 1.0f);
                float dSmooth = dProg * dProg * (3.0f - 2.0f * dProg);

                float satScale = 0.35f + (0.65f * dSmooth);
                glm::mat4 satRot = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f * dSmooth), glm::vec3(0, 1, 0));
                drawComponent(cube, satelliteBase, localSatPos, glm::vec3(0.85f, 0.65f, 0.65f) * satScale, gold, satRot);
                for (int side : {-1, 1}) {
                    glm::vec3 panelOffset = glm::vec3(satRot * glm::vec4(0, 0, side * 1.05f * satScale, 0));
                    drawComponent(cube, satelliteBase, localSatPos + panelOffset, glm::vec3(0.72f, 0.045f, 0.88f) * satScale, glm::vec3(0.035f, 0.12f, 0.34f), satRot);
                }
            }

            // ---------------- Exhaust ----------------
            // Self-lit, additive cones under each burning engine. The plume
            // widens with altitude as the outside air pressure drops.
            if (flight.launched && phase != PHASE_COAST) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glDepthMask(GL_FALSE);
                glUseProgram(rocketProgram);
                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 1.0f);

                float expansion = 1.0f + 1.2f * smoothStep(30.0f, 120.0f, altitude);
                float t = static_cast<float>(glfwGetTime());
                float flicker = 1.0f + 0.06f * sin(40.0f * t) + 0.04f * sin(97.0f * t);
                glm::mat4 flip = glm::rotate(glm::mat4(1.0f), PI, glm::vec3(1, 0, 0));

                auto drawPlume = [&](const glm::vec3& nozzle, float diameter, float length) {
                    float w = diameter * expansion;
                    float l = length * flicker * (1.0f + 0.4f * (expansion - 1.0f));
                    glUniform1f(glGetUniformLocation(rocketProgram, "alpha"), 0.55f);
                    drawComponent(cone, rocketTransform, nozzle - glm::vec3(0.0f, 0.5f * l, 0.0f), glm::vec3(w, l, w), glm::vec3(1.0f, 0.45f, 0.12f), flip);
                    glUniform1f(glGetUniformLocation(rocketProgram, "alpha"), 0.8f);
                    drawComponent(cone, rocketTransform, nozzle - glm::vec3(0.0f, 0.3f * l, 0.0f), glm::vec3(0.55f * w, 0.6f * l, 0.55f * w), glm::vec3(1.0f, 0.85f, 0.55f), flip);
                };

                if (phase == PHASE_PS1_STRAPONS) {
                    for (int i = 0; i < 4; ++i) {
                        float a = glm::radians(45.0f + 90.0f * i);
                        drawPlume(glm::vec3(0.92f * cos(a), 0.3f, 0.92f * sin(a)), 0.3f, 2.5f);
                    }
                }
                if (phase <= PHASE_PS1) drawPlume(glm::vec3(0.0f), 1.0f, 4.0f);
                else if (phase == PHASE_PS2) drawPlume(glm::vec3(0.0f, 3.65f, 0.0f), 0.8f, 3.0f);
                else if (phase == PHASE_PS3) drawPlume(glm::vec3(0.0f, 5.78f, 0.0f), 0.6f, 2.5f);
                else if (phase == PHASE_PS4) drawPlume(glm::vec3(0.0f, 7.5f, 0.0f), 0.4f, 1.5f);

                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 0.0f);
                glUniform1f(glGetUniformLocation(rocketProgram, "alpha"), 1.0f);
                glDepthMask(GL_TRUE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

            // ---------------- Flight path (Earth view) ----------------
            if (flight.launched && flight.missionTime - lastTrailTime >= 2.0) {
                trail.push_back(vehiclePosFixed);
                lastTrailTime = flight.missionTime;
            }
            if (cameraMode == 0 && !trail.empty()) {
                vector<glm::vec3> points = trail;
                points.push_back(vehiclePosFixed);
                glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
                glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), points.data(), GL_DYNAMIC_DRAW);

                setMatrices(rocketProgram, earthModel, view, projection);
                glUniform3f(glGetUniformLocation(rocketProgram, "objectColor"), 1.0f, 0.75f, 0.35f);
                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 1.0f);
                glBindVertexArray(trailVAO);
                glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(points.size()));
                glBindVertexArray(0);
                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 0.0f);
            }

            // ---------------- Status in the title bar ----------------
            if (currentFrame - lastTitleUpdate > 0.2) {
                lastTitleUpdate = currentFrame;
                char title[256];
                snprintf(title, sizeof(title),
                         "a2-scene | PSLV-C25 / Mangalyaan | T+%.0f s | alt %.1f km | v %.2f km/s | %s | warp %dx | camera: %s",
                         flight.missionTime, altitude, glm::length(flight.vehicle.vel),
                         phaseName(), WARP_LEVELS[warpIndex], CAMERA_NAMES[cameraMode]);
                glfwSetWindowTitle(window, title);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        earthMesh.reset();
        stars.reset();
        glDeleteBuffers(1, &trailVBO);
        glDeleteVertexArrays(1, &trailVAO);
        unitSphere.reset();
        glDeleteTextures(1, &earthTex);
        glDeleteTextures(1, &marsTex);
        if (groundTex) glDeleteTextures(1, &groundTex);
        glDeleteProgram(earthProgram);
        glDeleteProgram(atmosphereProgram);
        glDeleteProgram(rocketProgram);
        glDeleteProgram(groundProgram);
        glDeleteProgram(starProgram);
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << '\n';
    }

    glfwTerminate();
    return 0;
}
