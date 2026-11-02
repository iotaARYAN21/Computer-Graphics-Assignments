#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"
#include "../common/Mesh.h"
#include "../models/Cylinder.h"
#include "../models/Cube.h"
#include "../models/Fairing.h"
#include "../models/Plane.h"

#include <cmath>
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
const float EARTH_RADIUS = 30.0f;

// --- State Variables ---
int cameraMode = 1; // 0 = Earth View, 1 = Launch Site, 2 = Rocket View
float cameraYaw = 0.0f;
float cameraPitch = 15.0f;
float cameraDistance = 60.0f;

bool launchStarted = false;
double launchStartTime = 0.0;

bool boosterSeparationStarted = false;
double boosterSeparationStartTime = 0.0;
bool fairingSeparationStarted = false;
double fairingSeparationStartTime = 0.0;
bool stageSeparationStarted = false;
double stageSeparationStartTime = 0.0;
bool satelliteDeploymentStarted = false;
double satelliteDeploymentStartTime = 0.0;

// Earth Geometry
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

GLuint loadTexture(const char* path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* image = stbi_load(path, &width, &height, &channels, 0);
    if (!image) throw runtime_error(string("Cannot load texture: ") + path);
    
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

// Calculates a transformation matrix anchored to a specific Latitude/Longitude on a sphere
glm::mat4 getSurfaceTransform(float lat, float lon, float radius) {
    float latRad = glm::radians(lat);
    float lonRad = glm::radians(lon);
    
    glm::vec3 pos(
        radius * cos(latRad) * cos(lonRad),
        radius * sin(latRad),
        radius * cos(latRad) * sin(lonRad)
    );
    
    // Create a local coordinate system on the surface
    glm::vec3 up = glm::normalize(pos); 
    glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), up));
    if (glm::length(right) < 0.001f) right = glm::vec3(1, 0, 0); // Pole singularity fallback
    glm::vec3 forward = glm::normalize(glm::cross(up, right));
    
    glm::mat4 transform(1.0f);
    transform[0] = glm::vec4(right, 0.0f);
    transform[1] = glm::vec4(up, 0.0f);
    transform[2] = glm::vec4(forward, 0.0f);
    transform[3] = glm::vec4(pos, 1.0f);
    
    return transform;
}

void processInput(GLFWwindow* window) {
    const float angleStep = 1.2f;
    const float zoomStep = 1.0f;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    // Camera Controls - Orbit and Zoom
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) cameraYaw -= angleStep;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) cameraYaw += angleStep;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) cameraPitch = glm::min(cameraPitch + angleStep, 89.0f);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) cameraPitch = glm::max(cameraPitch - angleStep, -89.0f);
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) cameraDistance = glm::max(cameraDistance - zoomStep, 10.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) cameraDistance = glm::min(cameraDistance + zoomStep, 300.0f);

    // Edge-triggered key inputs for logic
    static bool cWasDown = false, lWasDown = false, bWasDown = false, fWasDown = false, sWasDown = false, dWasDown = false;
    bool cDown = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    bool lDown = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    bool bDown = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    bool fDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    bool sDown = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool dDown = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

    if (cDown && !cWasDown) {
        cameraMode = (cameraMode + 1) % 3; // Cycle cameras
        // Snap to sensible default distances for the newly selected mode
        if (cameraMode == 0) { cameraDistance = 110.0f; cameraPitch = 25.0f; } // Earth View
        if (cameraMode == 1) { cameraDistance = 50.0f; cameraPitch = 15.0f; }  // Launch Site View
        if (cameraMode == 2) { cameraDistance = 35.0f; cameraPitch = 20.0f; }  // Rocket View
    }

    if (lDown && !lWasDown && !launchStarted) { launchStarted = true; launchStartTime = glfwGetTime(); }
    if (bDown && !bWasDown && !boosterSeparationStarted) { boosterSeparationStarted = true; boosterSeparationStartTime = glfwGetTime(); }
    if (fDown && !fWasDown && !fairingSeparationStarted) { fairingSeparationStarted = true; fairingSeparationStartTime = glfwGetTime(); }
    if (sDown && !sWasDown && !stageSeparationStarted)   { stageSeparationStarted = true; stageSeparationStartTime = glfwGetTime(); }
    if (dDown && !dWasDown && !satelliteDeploymentStarted) { satelliteDeploymentStarted = true; satelliteDeploymentStartTime = glfwGetTime(); }
    
    cWasDown = cDown; lWasDown = lDown; bWasDown = bDown; fWasDown = fDown; sWasDown = sDown; dWasDown = dDown;
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "A2 Scene - Integrated Launch Mission", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    try {
        generateSphere(EARTH_RADIUS, 64, 64);
        auto earthMesh = make_unique<Mesh>(earthVertices, earthIndices);
        
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
        GLuint earthTex = loadTexture("textures/earth.jpg");

        float earthRot = 0.0f;
        double lastFrame = glfwGetTime();
        
        cout << "\n=== MISSION CONTROLS ==="
             << "\n  Arrows, [, ]: Orbit and Zoom Camera"
             << "\n  C: Switch Camera (Earth View / Launch Site / Rocket Cam)"
             << "\n  L: Liftoff!"
             << "\n  B: Separate Boosters"
             << "\n  S: Sequential Stage Separation"
             << "\n  F: Jettison Payload Fairing"
             << "\n  D: Deploy Satellite Payload\n========================\n\n";

        while (!glfwWindowShouldClose(window)) {
            processInput(window);
            double currentFrame = glfwGetTime();
            earthRot += (currentFrame - lastFrame) * 4.0f; // Slow Earth rotation
            lastFrame = currentFrame;

            // Compute Liftoff Altitude
            float rocketAltitude = 0.0f;
            if (launchStarted) {
                float t = currentFrame - launchStartTime;
                rocketAltitude = 2.0f * t * t; // Quadratic acceleration upwards
            }

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height ? height : 1);
            glClearColor(0.02f, 0.02f, 0.04f, 1.0f); // Deep space background
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // --- World Transformations ---
            glm::mat4 earthModel = glm::rotate(glm::mat4(1.0f), glm::radians(earthRot), glm::vec3(0,1,0));
            glm::mat4 localSiteTransform = getSurfaceTransform(13.7f, 80.2f, EARTH_RADIUS);
            glm::mat4 worldSiteTransform = earthModel * localSiteTransform; 
            
            glm::mat4 rocketTransform = glm::translate(worldSiteTransform, glm::vec3(0.0f, rocketAltitude, 0.0f));

            // Extracted Vectors for Cameras
            glm::vec3 sitePos = glm::vec3(worldSiteTransform[3]);
            glm::vec3 siteRight = glm::normalize(glm::vec3(worldSiteTransform[0]));
            glm::vec3 siteUp = glm::normalize(glm::vec3(worldSiteTransform[1]));
            glm::vec3 siteForward = glm::normalize(glm::vec3(worldSiteTransform[2]));
            
            glm::vec3 rocketPos = glm::vec3(rocketTransform[3]);
            glm::vec3 rocketRight = glm::normalize(glm::vec3(rocketTransform[0]));
            glm::vec3 rocketUp = glm::normalize(glm::vec3(rocketTransform[1]));
            glm::vec3 rocketForward = glm::normalize(glm::vec3(rocketTransform[2]));

            // Calculate Satellite Local Position early for camera tracking
            float dProg = satelliteDeploymentStarted ? glm::clamp((float)(glfwGetTime() - satelliteDeploymentStartTime)/3.0f, 0.0f, 1.0f) : 0.0f;
            float dSmooth = dProg * dProg * (3.0f - 2.0f * dProg);
            glm::vec3 localSatPos = glm::vec3(0.0f, 8.75f, 0.0f) + glm::vec3(0.0f, 8.0f * dSmooth, 0.0f); 
            // Convert local satellite position to world space
            glm::vec3 worldSatPos = glm::vec3(rocketTransform * glm::vec4(localSatPos, 1.0f));

            // --- 3 CAMERAS ---
            glm::mat4 view, projection;
            glm::vec3 cameraPos;
            projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 1500.0f);

            float cy = glm::radians(cameraYaw);
            float cp = glm::radians(cameraPitch);

            if (cameraMode == 0) {
                // Earth View
                cameraPos = glm::vec3(cameraDistance * cos(cp) * sin(cy), 
                                      cameraDistance * sin(cp), 
                                      cameraDistance * cos(cp) * cos(cy));
                view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            } else if (cameraMode == 1) {
                // Launch Site View
                glm::vec3 offset = siteRight * (cameraDistance * 0.4f * cos(cp) * sin(cy)) +
                                   siteUp * (cameraDistance * 0.4f * sin(cp)) +
                                   siteForward * (cameraDistance * 0.4f * cos(cp) * cos(cy));
                float upDot = glm::dot(offset, siteUp);
                if (upDot < 1.0f) offset += siteUp * (1.0f - upDot);

                cameraPos = sitePos + offset;
                glm::vec3 lookTarget = rocketPos + rocketUp * (rocketAltitude * 0.4f + 5.0f);
                view = glm::lookAt(cameraPos, lookTarget, siteUp);
            } else {
                // Rocket/Satellite View: Tracks the satellite specifically!
                glm::vec3 offset = rocketRight * (cameraDistance * 0.4f * cos(cp) * sin(cy)) +
                                   rocketUp * (cameraDistance * 0.4f * sin(cp)) +
                                   rocketForward * (cameraDistance * 0.4f * cos(cp) * cos(cy));

                cameraPos = worldSatPos + offset;
                view = glm::lookAt(cameraPos, worldSatPos, rocketUp);
            }

            glm::vec3 sunDir = glm::normalize(glm::vec3(1.0f, 0.5f, 1.0f));
            glm::vec3 lightPos = sunDir * 1000.0f; // Very distant sun

            // --- Draw Atmosphere ---
            glDepthMask(GL_FALSE);
            setMatrices(atmosphereProgram, glm::scale(earthModel, glm::vec3(1.03f)), view, projection);
            glUniform3fv(glGetUniformLocation(atmosphereProgram, "cameraPos"), 1, glm::value_ptr(cameraPos));
            earthMesh->draw();
            glDepthMask(GL_TRUE);

            // --- Draw Earth ---
            setMatrices(earthProgram, earthModel, view, projection);
            glUniform3f(glGetUniformLocation(earthProgram, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, earthTex);
            earthMesh->draw();

            // --- Draw Launch Site (Ground Plane & Pad) ---
            if (cameraMode == 1 || rocketAltitude < 150.0f) {
                glm::mat4 groundModel = glm::translate(worldSiteTransform, glm::vec3(0.0f, 0.05f, 0.0f)); 
                groundModel = glm::scale(groundModel, glm::vec3(0.4f, 1.0f, 0.4f)); 
                setMatrices(groundProgram, groundModel, view, projection);
                glUniform3f(glGetUniformLocation(groundProgram, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
                ground.draw();

                glUseProgram(rocketProgram);
                glm::mat4 padBase = glm::translate(worldSiteTransform, glm::vec3(0.0f, 0.18f + 0.05f, 0.0f));
                padBase = glm::scale(padBase, glm::vec3(6.0f, 0.36f, 5.0f));
                setMatrices(rocketProgram, padBase, view, projection);
                glUniform3f(glGetUniformLocation(rocketProgram, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
                glUniform3f(glGetUniformLocation(rocketProgram, "objectColor"), 0.48f, 0.49f, 0.52f);
                cube.draw();
            }

            auto drawComponent = [&](auto& drawable, const glm::vec3& localPos, const glm::vec3& scale, const glm::vec3& color, glm::mat4 customRot = glm::mat4(1.0f)) {
                glm::mat4 model = rocketTransform;
                model = glm::translate(model, localPos);
                model *= customRot;
                model = glm::scale(model, scale);
                setMatrices(rocketProgram, model, view, projection);
                glUniform3f(glGetUniformLocation(rocketProgram, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
                glUniform3fv(glGetUniformLocation(rocketProgram, "objectColor"), 1, glm::value_ptr(color));
                drawable.draw(); 
            };

            const glm::vec3 white(0.88f, 0.88f, 0.84f);
            const glm::vec3 orange(0.90f, 0.30f, 0.08f);
            const glm::vec3 dark(0.22f, 0.24f, 0.28f);
            const glm::vec3 gold(0.82f, 0.63f, 0.18f);

            // --- ROCKET STAGES ---
            float stageElapsed = stageSeparationStarted ? static_cast<float>(glfwGetTime() - stageSeparationStartTime) : 0.0f;
            auto getDrop = [&](float delay) {
                if (!stageSeparationStarted || stageElapsed <= delay) return 0.0f;
                float p = glm::clamp((stageElapsed - delay) / 2.0f, 0.0f, 1.0f);
                return 30.0f * (p * p); 
            };

            float drop1 = getDrop(0.0f);
            if (drop1 < 25.0f) {
                drawComponent(cylinder, glm::vec3(0.0f, 1.75f - drop1, 0.0f), glm::vec3(1.45f, 3.5f, 1.45f), white);
                drawComponent(cylinder, glm::vec3(0.0f, 3.58f - drop1, 0.0f), glm::vec3(1.48f, 0.18f, 1.48f), dark); 
            }

            float drop2 = getDrop(1.5f);
            if (drop2 < 25.0f) {
                drawComponent(cylinder, glm::vec3(0.0f, 4.65f - drop2, 0.0f), glm::vec3(1.12f, 2.0f, 1.12f), orange);
                drawComponent(cylinder, glm::vec3(0.0f, 5.72f - drop2, 0.0f), glm::vec3(1.16f, 0.16f, 1.16f), dark); 
            }

            float drop3 = getDrop(3.0f);
            if (drop3 < 25.0f) {
                drawComponent(cylinder, glm::vec3(0.0f, 6.65f - drop3, 0.0f), glm::vec3(0.88f, 1.75f, 0.88f), white);
            }

            drawComponent(cylinder, glm::vec3(0.0f, 7.78f, 0.0f), glm::vec3(0.72f, 0.55f, 0.72f), dark); 
            drawComponent(cylinder, glm::vec3(0.0f, 8.18f, 0.0f), glm::vec3(0.62f, 0.28f, 0.62f), gold); 

            // --- BOOSTERS ---
            float bProg = boosterSeparationStarted ? glm::clamp((float)(glfwGetTime() - boosterSeparationStartTime)/2.0f, 0.0f, 1.0f) : 0.0f;
            for (int i = 0; i < 4; ++i) {
                float a = glm::radians(45.0f + 90.0f * i);
                glm::vec3 outDir(cos(a), 0.0f, sin(a));
                glm::vec3 pos = glm::vec3(0.92f * cos(a), 1.85f - drop1, 0.92f * sin(a)) + (outDir * (3.5f * bProg)) - glm::vec3(0, 10.0f * bProg * bProg, 0);
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(60.0f * bProg), glm::cross(glm::vec3(0,1,0), outDir));
                if (drop1 < 25.0f) {
                    drawComponent(cylinder, pos, glm::vec3(0.38f, 3.1f, 0.38f), white, rot);
                    drawComponent(cone, pos + glm::vec3(0, 1.65f, 0), glm::vec3(0.38f, 0.48f, 0.38f), orange, rot);
                }
            }

            // --- FAIRING ---
            float fProg = fairingSeparationStarted ? glm::clamp((float)(glfwGetTime() - fairingSeparationStartTime)/2.0f, 0.0f, 1.0f) : 0.0f;
            float fSmooth = fProg * fProg * (3.0f - 2.0f * fProg);
            glm::vec3 fairingScale(1.22f, 1.55f, 1.22f);
            glm::vec3 fairingBasePos(0.0f, 9.05f, 0.0f);
            
            if (fProg < 0.99f) { 
                glm::mat4 rotA = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f * fSmooth), glm::vec3(1, 0, 0)); 
                glm::vec3 posA = fairingBasePos + glm::vec3(0.0f, -10.0f * (fProg * fProg), 2.5f * fSmooth);
                drawComponent(fairingA, posA, fairingScale, white, rotA);
                
                glm::mat4 rotB = glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f * fSmooth), glm::vec3(1, 0, 0));
                glm::vec3 posB = fairingBasePos + glm::vec3(0.0f, -10.0f * (fProg * fProg), -2.5f * fSmooth);
                drawComponent(fairingB, posB, fairingScale, white, rotB);
            }

            // --- SATELLITE ---
            float satScale = 0.35f + (0.65f * dSmooth);
            glm::mat4 satRot = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f * dSmooth), glm::vec3(0, 1, 0));
            
            if (fairingSeparationStarted) {
                drawComponent(cube, localSatPos, glm::vec3(0.85f, 0.65f, 0.65f) * satScale, gold, satRot);
                for (int side : {-1, 1}) {
                    glm::vec3 panelOffset = glm::vec3(satRot * glm::vec4(0, 0, side * 1.05f * satScale, 0));
                    drawComponent(cube, localSatPos + panelOffset, glm::vec3(0.72f, 0.045f, 0.88f) * satScale, glm::vec3(0.035f, 0.12f, 0.34f), satRot);
                }
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << '\n';
    }

    glfwTerminate();
    return 0;
}