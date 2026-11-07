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
#include "Keyframes.h"
#include "SceneConfig.h"
#include "TrajectoryEditor.h"

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

//       
// Cameras
//       
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

//       
// Keyframe animation (trajectory.txt + keyframes.txt)
//       
const int FRAMES_PER_INTERVAL = 60; // fixed number of frames per keyframe interval
vector<ControlPoints> trajectories;
vector<Keyframe> keyframes;
vector<float> separationTilt(TRAJ_COUNT, 0.0f); // vehicle tilt when each part separated
bool animationReady = false;
bool playing = false;
int frame = 0;
bool userCamera = false; // C switches from the keyframed camera to the manual ones

void loadAnimation() {
    trajectories = loadTrajectories("trajectory.txt");
    keyframes = loadKeyframes("keyframes.txt", TRAJ_COUNT, FIELD_COUNT);
    animationReady = trajectories.size() >= TRAJ_COUNT && !keyframes.empty();
    if (!animationReady) {
        cout << "Animation needs " << TRAJ_COUNT << " trajectories in trajectory.txt and keyframes.txt "
             << "(found " << trajectories.size() << " and " << keyframes.size() << "). Run ./build/a3-plan.\n";
        return;
    }
    for (int id = 0; id < TRAJ_COUNT; ++id)
        for (const Keyframe& k : keyframes)
            if (k.active[id]) { separationTilt[id] = k.extra[FIELD_TILT]; break; }
    cout << "Loaded " << trajectories.size() << " trajectories and " << keyframes.size() << " keyframes.\n";
}

// Vehicle axes at a position: long axis tilted from the local vertical by
// tiltDeg, inside the flight plane of the launch.
glm::mat3 frameAt(const glm::vec3& pos, float tiltDeg) {
    static const glm::vec3 planeNormal = [] {
        glm::vec3 up = surfaceDirection(SITE_LATITUDE, SITE_LONGITUDE);
        glm::vec3 east = glm::normalize(glm::cross(glm::vec3(0, 1, 0), up));
        glm::vec3 north = glm::cross(up, east);
        float az = glm::radians(float(LAUNCH_AZIMUTH));
        return glm::normalize(glm::cross(up, cos(az) * north + sin(az) * east));
    }();
    glm::vec3 up = glm::normalize(pos);
    glm::vec3 forward = glm::normalize(glm::cross(planeNormal, up));
    float t = glm::radians(tiltDeg);
    glm::vec3 axis = cos(t) * up + sin(t) * forward;
    glm::vec3 x = glm::normalize(planeNormal - glm::dot(planeNormal, axis) * axis);
    return glm::mat3(x, axis, glm::cross(x, axis));
}

// Bezier trajectory editor (E toggles it) and queued mouse clicks.
TrajectoryEditor editor;
bool editorNeedsFocus = true;
struct MouseClick { double x, y; int button; };
vector<MouseClick> pendingClicks;

double cursorX = 0.0, cursorY = 0.0;

void cursorPosCallback(GLFWwindow*, double x, double y) {
    cursorX = x;
    cursorY = y;
}

void mouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (action != GLFW_PRESS || !editor.active) return;
    pendingClicks.push_back({cursorX, cursorY, button});
}

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
    playing = false;
    frame = 0;
    // Point the Earth-view camera at India.
    glm::vec3 d = surfaceDirection(SITE_LATITUDE, SITE_LONGITUDE);
    cameras[0].pitch = glm::degrees(asin(d.y)) + 10.0f;
    cameras[0].yaw = glm::degrees(atan2(d.x, d.z));
}

void processInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    if (editor.active) {
        // Editor camera: arrows orbit, Shift+arrows pan, PgUp/PgDn depth, [ ] zoom.
        editor.updateMovement(window, deltaTime);
    } else {
        // Camera orbit and zoom for the active scene camera.
        OrbitCamera& cam = cameras[cameraMode];
        const float angleSpeed = 60.0f * deltaTime;
        const float zoomFactor = 1.0f + 1.5f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) cam.yaw -= angleSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) cam.yaw += angleSpeed;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) cam.pitch = glm::min(cam.pitch + angleSpeed, 89.0f);
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) cam.pitch = glm::max(cam.pitch - angleSpeed, cam.minPitch);
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) cam.distance = glm::max(cam.distance / zoomFactor, cam.minDistance);
        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) cam.distance = glm::min(cam.distance * zoomFactor, cam.maxDistance);
    }

    // Edge-triggered keys.
    static bool wasDown[GLFW_KEY_LAST + 1] = {false};
    auto pressed = [&](int key) {
        bool down = glfwGetKey(window, key) == GLFW_PRESS;
        bool result = down && !wasDown[key];
        wasDown[key] = down;
        return result;
    };

    if (pressed(GLFW_KEY_E)) {
        editor.active = !editor.active;
        editor.status = editor.active ? "editor on" : "";
        pendingClicks.clear();
    }
    if (editor.active) {
        if (pressed(GLFW_KEY_T)) editor.saveCurrent();
        if (pressed(GLFW_KEY_N)) editor.newTrajectory();
        if (pressed(GLFW_KEY_BACKSPACE)) editor.removeLastPoint();
        if (pressed(GLFW_KEY_F)) editor.focusOnLastPoint();
    }
    // Y and H also work outside the editor, so curves can be viewed from
    // the normal cameras.
    if (pressed(GLFW_KEY_Y)) editor.loadAll();
    if (pressed(GLFW_KEY_H)) editor.showLoaded = !editor.showLoaded;

    if (!editor.active && pressed(GLFW_KEY_C)) {
        if (userCamera) cameraMode = (cameraMode + 1) % CAMERA_COUNT;
        userCamera = true;
    }
    if (pressed(GLFW_KEY_P)) { // start the animation from the first keyframe
        loadAnimation();
        resetMission();
        playing = animationReady;
        userCamera = false;
    }
    if (pressed(GLFW_KEY_SPACE) && animationReady) playing = !playing;
    if (pressed(GLFW_KEY_R)) resetMission();
}

const char* stageName(int stage, bool engineOn) {
    const char* names[] = {"PS1 + strap-ons", "PS1", "PS2", "PS2", "PS3", "PS4", "Mangalyaan separated"};
    if (stage == STAGE_ON_PAD && !engineOn) return "on pad";
    if (!engineOn && stage < STAGE_PAYLOAD_SEPARATED) return "coast";
    return names[glm::clamp(stage, 0, 6)];
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "a3-scene - PSLV-C25 / Mangalyaan", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
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
        GLuint groundTex = loadTexture("textures/groundTexture.jpg", true);
        if (!groundTex) cerr << "Note: textures/groundTexture.jpg not found, using a plain ground colour.\n";

        loadAnimation();
        resetMission();

        cout << "\n=== a3-scene: PSLV-C25 / Mangalyaan ==="
             << "\n  P        : start the animation (reloads trajectory.txt and keyframes.txt)"
             << "\n  Space    : pause / resume"
             << "\n  R        : back to the first keyframe"
             << "\n  C        : manual cameras (Earth view / Launch site / Vehicle / Planets); P returns to the keyframed camera"
             << "\n  Arrows   : orbit the manual camera,  [ and ] : zoom"
             << "\n  Esc      : exit"
             << "\n"
             << "\n  Trajectory editor"
             << "\n  E            : editor on / off"
             << "\n  Left click   : add a control point (on the focus plane, or on Earth)"
             << "\n  Right click / Backspace : remove the last point"
             << "\n  Arrows       : orbit the editor camera around the focus point"
             << "\n  Shift+Arrows : move the focus point across the screen"
             << "\n  PgUp / PgDn  : move the focus point into / out of the screen"
             << "\n  [ and ]      : zoom"
             << "\n  F            : focus on the last point"
             << "\n  N            : start a new trajectory"
             << "\n  T            : save (append) the current trajectory to trajectory.txt"
             << "\n  Y            : load all trajectories from trajectory.txt and show them"
             << "\n  H            : hide / show the loaded trajectories"
             << "\n========================================\n\n";

        double lastFrame = glfwGetTime();
        double lastTitleUpdate = 0.0;

        while (!glfwWindowShouldClose(window)) {
            double currentFrame = glfwGetTime();
            float deltaTime = static_cast<float>(currentFrame - lastFrame);
            lastFrame = currentFrame;
            processInput(window, deltaTime);

            // ---------------- Animation state ----------------
            FrameState st;
            if (animationReady) {
                st = interpolateKeyframes(keyframes, frame, FRAMES_PER_INTERVAL, fieldIsDiscrete);
                int last = totalFrames(static_cast<int>(keyframes.size()), FRAMES_PER_INTERVAL) - 1;
                if (playing && ++frame > last) { frame = last; playing = false; }
            }
            auto onPath = [&](int id, glm::vec3& p) {
                if (!animationReady || !st.active[id]) return false;
                p = evaluateBezier(trajectories[id], st.u[id]);
                return true;
            };
            int stage = animationReady ? static_cast<int>(st.extra[FIELD_STAGE] + 0.5f) : STAGE_ON_PAD;
            bool engineOn = animationReady && st.extra[FIELD_ENGINE] > 0.5f;
            float tilt = animationReady ? st.extra[FIELD_TILT] : 0.0f;

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            if (height == 0) height = 1;
            glViewport(0, 0, width, height);

            // ---------------- World transforms ----------------
            glm::mat4 earthModel(1.0f);
            glm::mat4 worldSiteTransform = getSurfaceTransform(SITE_LATITUDE, SITE_LONGITUDE, EARTH_RADIUS);
            glm::vec3 sitePos = glm::vec3(worldSiteTransform[3]);
            glm::vec3 siteRight = glm::vec3(worldSiteTransform[0]);
            glm::vec3 siteUp = glm::vec3(worldSiteTransform[1]);
            glm::vec3 siteForward = glm::vec3(worldSiteTransform[2]);

            // Vehicle base position (on the pad until the animation moves it).
            glm::vec3 vehiclePos = sitePos + siteUp * (PAD_TOP * ROCKET_SCALE);
            onPath(TRAJ_VEHICLE, vehiclePos);
            float altitude = glm::max((glm::length(vehiclePos) - EARTH_RADIUS - PAD_TOP * ROCKET_SCALE) / KM_TO_WORLD, 0.0f);

            auto placeFrame = [&](const glm::vec3& pos, const glm::mat3& frame) {
                glm::mat4 m(frame);
                m[3] = glm::vec4(pos, 1.0f);
                return m * glm::scale(glm::mat4(1.0f), glm::vec3(ROCKET_SCALE));
            };

            glm::mat4 rocketTransform = placeFrame(vehiclePos, frameAt(vehiclePos, tilt));
            glm::vec3 rocketRight = glm::normalize(glm::vec3(rocketTransform[0]));
            glm::vec3 rocketUp = glm::normalize(glm::vec3(rocketTransform[1]));
            glm::vec3 rocketForward = glm::normalize(glm::vec3(rocketTransform[2]));
            // Middle of whatever is left of the stack, used as camera target.
            const float STACK_CENTRE[] = {4.9f, 4.9f, 6.2f, 6.2f, 7.5f, 8.2f, 8.2f};
            glm::vec3 rocketCentre = glm::vec3(rocketTransform * glm::vec4(0.0f, STACK_CENTRE[glm::clamp(stage, 0, 6)], 0.0f, 1.0f));

            glm::vec3 payloadPos;
            bool payloadSeparated = stage == STAGE_PAYLOAD_SEPARATED && onPath(TRAJ_PAYLOAD, payloadPos);
            glm::mat4 satelliteBase = payloadSeparated
                ? placeFrame(payloadPos, frameAt(payloadPos, separationTilt[TRAJ_PAYLOAD])) : rocketTransform;
            const glm::vec3 localSatPos(0.0f, 8.75f, 0.0f);
            glm::vec3 worldSatPos = glm::vec3(satelliteBase * glm::vec4(localSatPos, 1.0f));

            // After the launch, Mangalyaan follows one curve per mission phase,
            // pointing along its direction of travel and drawn larger.
            int spacePath = -1;
            for (int id : {TRAJ_ORBIT_RAISING, TRAJ_CRUISE, TRAJ_MARS_ORBIT})
                if (animationReady && st.active[id]) spacePath = id;
            bool vehicleVisible = !animationReady || st.active[TRAJ_VEHICLE];
            if (spacePath >= 0) {
                const ControlPoints& c = trajectories[spacePath];
                float u = st.u[spacePath];
                glm::vec3 pos = evaluateBezier(c, u);
                glm::vec3 axis = glm::normalize(evaluateBezier(c, glm::min(u + 0.002f, 1.0f)) - evaluateBezier(c, glm::max(u - 0.002f, 0.0f)));
                glm::vec3 x = glm::normalize(glm::cross(axis, glm::vec3(0.0f, 1.0f, 0.0f)));
                satelliteBase = glm::translate(glm::mat4(1.0f), pos) * glm::mat4(glm::mat3(x, axis, glm::cross(x, axis)))
                              * glm::scale(glm::mat4(1.0f), glm::vec3(ROCKET_SCALE * PAYLOAD_SPACE_SCALE))
                              * glm::translate(glm::mat4(1.0f), -localSatPos);
                worldSatPos = pos;
                payloadSeparated = true;
            }

            // ---------------- Cameras ----------------
            OrbitCamera& cam = cameras[cameraMode];
            float cy = glm::radians(cam.yaw);
            float cp = glm::radians(cam.pitch);
            glm::vec3 orbit(cos(cp) * sin(cy), sin(cp), cos(cp) * cos(cy));

            // Active keyframed camera: an eye trajectory followed by its target.
            int keyCameraEye = -1;
            for (int id : {TRAJ_CAMERA_EYE, TRAJ_EYE_ORBIT, TRAJ_EYE_CRUISE, TRAJ_EYE_MARS})
                if (animationReady && st.active[id] && st.active[id + 1]) keyCameraEye = id;

            glm::mat4 view;
            glm::vec3 cameraPos;
            float nearPlane = 0.0005f;
            float farPlane = 2000.0f;

            if (editor.active && editorNeedsFocus) {
                // First time in the editor: start just above the launch pad.
                editor.focus = sitePos + siteUp * 0.03f;
                editor.distance = 0.15f;
                editorNeedsFocus = false;
            }

            if (editor.active) {
                cameraPos = editor.cameraPosition();
                view = editor.viewMatrix();
                nearPlane = editor.nearPlane();
                farPlane = 6000.0f;
            } else if (!userCamera && keyCameraEye >= 0) {
                // Keyframed camera: eye and look-at target follow their own curves.
                cameraPos = evaluateBezier(trajectories[keyCameraEye], st.u[keyCameraEye]);
                glm::vec3 target = evaluateBezier(trajectories[keyCameraEye + 1], st.u[keyCameraEye + 1]);
                glm::vec3 up = keyCameraEye == TRAJ_CAMERA_EYE ? glm::normalize(target) : glm::vec3(0.0f, 1.0f, 0.0f);
                if (fabs(glm::dot(glm::normalize(target - cameraPos), up)) > 0.95f) up = glm::vec3(1.0f, 0.0f, 0.0f);
                view = glm::lookAt(cameraPos, target, up);
                if (keyCameraEye != TRAJ_CAMERA_EYE) nearPlane = 0.05f;
            } else if (cameraMode == 0) {
                // Earth view: distant camera around Earth's centre.
                cameraPos = orbit * cam.distance;
                view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                nearPlane = 0.5f;
            } else if (cameraMode == 1) {
                // Launch-site camera: fixed near the pad, tracks the vehicle.
                float d = cam.distance * ROCKET_SCALE;
                glm::vec3 offset = siteRight * (d * orbit.x) + siteUp * (d * orbit.y) + siteForward * (d * orbit.z);
                cameraPos = sitePos + offset;
                glm::vec3 target = payloadSeparated ? worldSatPos : rocketCentre;
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
                glm::vec3 target = payloadSeparated ? worldSatPos : rocketCentre;
                float d = cam.distance * ROCKET_SCALE;
                if (spacePath >= 0) {
                    // Mangalyaan is drawn larger after the launch: orbit it instead.
                    d *= 0.25f * PAYLOAD_SPACE_SCALE;
                    rocketRight = glm::normalize(glm::vec3(satelliteBase[0]));
                    rocketUp = glm::normalize(glm::vec3(satelliteBase[1]));
                    rocketForward = glm::normalize(glm::vec3(satelliteBase[2]));
                    nearPlane = 0.05f;
                }
                glm::vec3 offset = rocketRight * (d * orbit.x) + rocketUp * (d * orbit.y) + rocketForward * (d * orbit.z);
                cameraPos = target + offset;
                view = glm::lookAt(cameraPos, target, rocketUp);
            }
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / height, nearPlane, farPlane);

            // Mouse clicks for the editor need this frame's projection.
            if (editor.active) {
                int windowWidth, windowHeight;
                glfwGetWindowSize(window, &windowWidth, &windowHeight);
                for (const MouseClick& click : pendingClicks) {
                    if (click.button == GLFW_MOUSE_BUTTON_LEFT)
                        editor.placePoint(click.x, click.y, windowWidth, windowHeight, projection, EARTH_RADIUS);
                    else if (click.button == GLFW_MOUSE_BUTTON_RIGHT)
                        editor.removeLastPoint();
                }
            }
            pendingClicks.clear();

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

                // Mars: textured, lit by the Sun. When the camera is close to
                // Mars it is drawn with the normal depth range instead, so
                // the spacecraft can pass behind it.
                glm::vec3 marsDir = surfaceDirection(MARS_LATITUDE, MARS_LONGITUDE);
                bool marsNear = glm::length(cameraPos - marsDir * MARS_DISTANCE) < 300.0f;
                if (marsNear) glClear(GL_DEPTH_BUFFER_BIT);
                glm::mat4 marsModel = glm::translate(glm::mat4(1.0f), marsDir * MARS_DISTANCE);
                marsModel = glm::scale(marsModel, glm::vec3(MARS_RADIUS));
                setMatrices(earthProgram, marsModel, view, marsNear ? projection : farProjection);
                glUniform3fv(glGetUniformLocation(earthProgram, "lightPos"), 1, glm::value_ptr(lightPos));
                glUniform1i(glGetUniformLocation(earthProgram, "earthTexture"), 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, marsTex);
                unitSphere->draw();
                glBindTexture(GL_TEXTURE_2D, 0);

                if (!marsNear) glClear(GL_DEPTH_BUFFER_BIT);
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
                glUniform1f(glGetUniformLocation(groundProgram, "uvRepeat"), 1.0f);
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

            if (vehicleVisible) {
                if (stage == STAGE_ON_PAD)
                    for (int i = 0; i < 4; ++i) drawBooster(rocketTransform, i, glm::mat4(1.0f));
                if (stage <= STAGE_NO_BOOSTERS) drawStage1(rocketTransform);
                if (stage <= STAGE_NO_FAIRING) drawStage2(rocketTransform);
                if (stage <= STAGE_NO_PS2) drawStage3(rocketTransform);

                // PS4 and payload adapter stay with the vehicle.
                drawComponent(cylinder, rocketTransform, glm::vec3(0.0f, 7.78f, 0.0f), glm::vec3(0.72f, 0.55f, 0.72f), dark);
                drawComponent(cylinder, rocketTransform, glm::vec3(0.0f, 8.18f, 0.0f), glm::vec3(0.62f, 0.28f, 0.62f), gold);

                if (stage <= STAGE_NO_PS1) {
                    drawComponent(fairingA, rocketTransform, fairingBasePos, fairingScale, white);
                    drawComponent(fairingB, rocketTransform, fairingBasePos, fairingScale, white);
                }
            }

            // ---------------- Separated parts (each on its own curve) ----------------
            // They keep the attitude they had at separation and tumble slowly.
            // Boosters and fairing halves also drift sideways from their group path
            // (spring push of 12 m/s and 6 m/s, drawn at the vehicle's scale).
            glm::vec3 p;
            auto partBase = [&](int id) { return placeFrame(p, frameAt(p, separationTilt[id])); };
            if (onPath(TRAJ_BOOSTERS, p)) {
                float u = st.u[TRAJ_BOOSTERS];
                float spread = 12.0f * DEBRIS_WINDOW_S * u * MODEL_UNITS_PER_METRE;
                for (int i = 0; i < 4; ++i) {
                    float a = glm::radians(45.0f + 90.0f * i);
                    glm::vec3 outDir(cos(a), 0.0f, sin(a));
                    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(70.0f * u), glm::cross(glm::vec3(0, 1, 0), outDir));
                    drawBooster(glm::translate(partBase(TRAJ_BOOSTERS), outDir * spread), i, rot);
                }
            }
            if (onPath(TRAJ_STAGE1, p)) drawStage1(glm::rotate(partBase(TRAJ_STAGE1), glm::radians(25.0f * st.u[TRAJ_STAGE1]), glm::vec3(1, 0, 0)));
            if (onPath(TRAJ_STAGE2, p)) drawStage2(glm::rotate(partBase(TRAJ_STAGE2), glm::radians(25.0f * st.u[TRAJ_STAGE2]), glm::vec3(1, 0, 0)));
            if (onPath(TRAJ_STAGE3, p)) drawStage3(glm::rotate(partBase(TRAJ_STAGE3), glm::radians(25.0f * st.u[TRAJ_STAGE3]), glm::vec3(1, 0, 0)));
            if (onPath(TRAJ_FAIRING, p)) {
                // Each half swings open about its base while drifting sideways.
                float u = st.u[TRAJ_FAIRING];
                for (float sign : {1.0f, -1.0f}) {
                    glm::mat4 hinge = glm::translate(partBase(TRAJ_FAIRING),
                        fairingBasePos + glm::vec3(0.0f, -0.5f * fairingScale.y, sign * 6.0f * DEBRIS_WINDOW_S * u * MODEL_UNITS_PER_METRE));
                    hinge = glm::rotate(hinge, glm::radians(sign * 120.0f * u), glm::vec3(1, 0, 0));
                    drawComponent(sign > 0 ? fairingA : fairingB, hinge, glm::vec3(0.0f, 0.5f * fairingScale.y, 0.0f), fairingScale, white);
                }
            }

            // ---------------- Mangalyaan ----------------
            if (stage >= STAGE_NO_FAIRING) {
                // Unfolds during the first part of its own path.
                float dProg = payloadSeparated ? glm::clamp(st.u[TRAJ_PAYLOAD] * 8.0f, 0.0f, 1.0f) : 0.0f;
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
            if (engineOn) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glDepthMask(GL_FALSE);
                glUseProgram(rocketProgram);
                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 1.0f);

                float expansion = 1.0f + 1.2f * smoothStep(30.0f, 120.0f, altitude);
                float t = static_cast<float>(glfwGetTime());
                float flicker = 1.0f + 0.06f * sin(40.0f * t) + 0.04f * sin(97.0f * t);
                glm::mat4 flip = glm::rotate(glm::mat4(1.0f), PI, glm::vec3(1, 0, 0));

                glm::mat4 plumeBase = rocketTransform;
                auto drawPlume = [&](const glm::vec3& nozzle, float diameter, float length) {
                    float w = diameter * expansion;
                    float l = length * flicker * (1.0f + 0.4f * (expansion - 1.0f));
                    glUniform1f(glGetUniformLocation(rocketProgram, "alpha"), 0.55f);
                    drawComponent(cone, plumeBase, nozzle - glm::vec3(0.0f, 0.5f * l, 0.0f), glm::vec3(w, l, w), glm::vec3(1.0f, 0.45f, 0.12f), flip);
                    glUniform1f(glGetUniformLocation(rocketProgram, "alpha"), 0.8f);
                    drawComponent(cone, plumeBase, nozzle - glm::vec3(0.0f, 0.3f * l, 0.0f), glm::vec3(0.55f * w, 0.6f * l, 0.55f * w), glm::vec3(1.0f, 0.85f, 0.55f), flip);
                };

                if (spacePath >= 0) {
                    // Mangalyaan's own engine (orbit-raising, TMI and MOI burns).
                    plumeBase = satelliteBase;
                    drawPlume(localSatPos - glm::vec3(0.0f, 0.35f, 0.0f), 0.25f, 0.8f);
                } else if (stage == STAGE_ON_PAD) {
                    for (int i = 0; i < 4; ++i) {
                        float a = glm::radians(45.0f + 90.0f * i);
                        drawPlume(glm::vec3(0.92f * cos(a), 0.3f, 0.92f * sin(a)), 0.3f, 2.5f);
                    }
                }
                if (spacePath >= 0) {}
                else if (stage <= STAGE_NO_BOOSTERS) drawPlume(glm::vec3(0.0f), 1.0f, 4.0f);
                else if (stage <= STAGE_NO_FAIRING) drawPlume(glm::vec3(0.0f, 3.65f, 0.0f), 0.8f, 3.0f);
                else if (stage == STAGE_NO_PS2) drawPlume(glm::vec3(0.0f, 5.78f, 0.0f), 0.6f, 2.5f);
                else drawPlume(glm::vec3(0.0f, 7.5f, 0.0f), 0.4f, 1.5f);

                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 0.0f);
                glUniform1f(glGetUniformLocation(rocketProgram, "alpha"), 1.0f);
                glDepthMask(GL_TRUE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

            // ---------------- Path so far: vehicle (manual Earth view) or Mangalyaan ----------------
            int trailPath = spacePath >= 0 ? spacePath
                          : (userCamera && cameraMode == 0 && animationReady && st.active[TRAJ_VEHICLE]) ? TRAJ_VEHICLE : -1;
            if (trailPath >= 0) {
                vector<glm::vec3> points;
                for (int i = 0; i <= 1000; ++i)
                    points.push_back(evaluateBezier(trajectories[trailPath], st.u[trailPath] * i / 1000.0f));
                glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
                glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), points.data(), GL_DYNAMIC_DRAW);
                setMatrices(rocketProgram, glm::mat4(1.0f), view, projection);
                glUniform3f(glGetUniformLocation(rocketProgram, "objectColor"), 1.0f, 0.75f, 0.35f);
                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 1.0f);
                glBindVertexArray(trailVAO);
                glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(points.size()));
                glBindVertexArray(0);
                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 0.0f);
            }

            // ---------------- Trajectories (editor and loaded curves) ----------------
            if (editor.active || (editor.showLoaded && !editor.loaded.empty())) {
                auto drawLines = [&](const vector<glm::vec3>& points, const glm::vec3& colour, GLenum mode) {
                    if (points.size() < 2) return;
                    glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
                    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), points.data(), GL_DYNAMIC_DRAW);
                    setMatrices(rocketProgram, glm::mat4(1.0f), view, projection);
                    glUniform3fv(glGetUniformLocation(rocketProgram, "objectColor"), 1, glm::value_ptr(colour));
                    glBindVertexArray(trailVAO);
                    glDrawArrays(mode, 0, static_cast<GLsizei>(points.size()));
                    glBindVertexArray(0);
                };
                // Small sphere whose on-screen size stays roughly constant.
                auto drawMarker = [&](const glm::vec3& pos, float size, const glm::vec3& colour) {
                    float radius = size * glm::length(cameraPos - pos);
                    glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), pos), glm::vec3(radius));
                    setMatrices(rocketProgram, model, view, projection);
                    glUniform3fv(glGetUniformLocation(rocketProgram, "objectColor"), 1, glm::value_ptr(colour));
                    unitSphere->draw();
                };

                glUseProgram(rocketProgram);
                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 1.0f);

                if (editor.showLoaded) {
                    for (size_t i = 0; i < editor.loaded.size(); ++i) {
                        const ControlPoints& pts = editor.loaded[i];
                        glm::vec3 colour = trajectoryColour(static_cast<int>(i));
                        drawLines(pts, colour * 0.45f, GL_LINE_STRIP);          // control polygon
                        drawLines(sampleBezier(pts, 300), colour, GL_LINE_STRIP); // curve
                        for (const glm::vec3& p : pts) drawMarker(p, 0.006f, colour);
                    }
                }

                if (editor.active) {
                    const glm::vec3 pointColour(1.0f, 1.0f, 0.3f);
                    drawLines(editor.current, glm::vec3(0.55f), GL_LINE_STRIP);
                    drawLines(sampleBezier(editor.current, 300), pointColour, GL_LINE_STRIP);
                    for (size_t i = 0; i < editor.current.size(); ++i) {
                        bool last = (i + 1 == editor.current.size());
                        drawMarker(editor.current[i], last ? 0.011f : 0.008f, last ? glm::vec3(1.0f) : pointColour);
                    }

                    // Focus point: small cross along the world axes.
                    float c = 0.04f * editor.distance;
                    glm::vec3 f = editor.focus;
                    vector<glm::vec3> cross = {
                        f - glm::vec3(c, 0, 0), f + glm::vec3(c, 0, 0),
                        f - glm::vec3(0, c, 0), f + glm::vec3(0, c, 0),
                        f - glm::vec3(0, 0, c), f + glm::vec3(0, 0, c)};
                    drawLines(cross, glm::vec3(0.3f, 1.0f, 0.4f), GL_LINES);
                }

                glUniform1f(glGetUniformLocation(rocketProgram, "emissive"), 0.0f);
            }

            // ---------------- Status in the title bar ----------------
            if (currentFrame - lastTitleUpdate > 0.2) {
                lastTitleUpdate = currentFrame;
                char title[320];
                if (editor.active) {
                    snprintf(title, sizeof(title),
                             "a3-scene | TRAJECTORY EDITOR | points: %zu | focus (%.3f, %.3f, %.3f) | view dist %.3f | %s",
                             editor.current.size(), editor.focus.x, editor.focus.y, editor.focus.z,
                             editor.distance, editor.status.c_str());
                } else {
                    char alt[32] = "";
                    if (spacePath < 0) snprintf(alt, sizeof(alt), " | alt %.1f km", altitude);
                    snprintf(title, sizeof(title),
                             "a3-scene | %s | frame %d (keyframe %d of %zu)%s | %s | camera: %s",
                             playing ? "PLAYING" : (animationReady ? "press P to play" : "no keyframes"),
                             frame, frame / FRAMES_PER_INTERVAL + 1, keyframes.size(), alt,
                             spacePath == TRAJ_ORBIT_RAISING ? (engineOn ? "Earth orbit raising: perigee burn" : "Earth orbit raising")
                             : spacePath == TRAJ_CRUISE ? (engineOn ? (st.u[TRAJ_CRUISE] < 0.5f ? "Trans-Mars Injection burn" : "Mars Orbit Insertion burn") : "cruise to Mars")
                             : spacePath == TRAJ_MARS_ORBIT ? "in Mars orbit" : stageName(stage, engineOn),
                             userCamera ? CAMERA_NAMES[cameraMode] : "keyframed");
                }
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
