#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../models/Cylinder.h"
#include "../models/Cube.h"
#include "../models/Fairing.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

// =====================================================================
// HOW ANIMATION WORKS IN THIS FILE (a2-model-2)  -- read this first
// =====================================================================
// Nothing here is "moved" step by step. Instead, EVERY FRAME we:
//   1. ask "how many seconds since the user pressed the key?"
//        elapsed = glfwGetTime() - startTime
//   2. turn that into a progress number between 0 and 1
//        progress = clamp(elapsed / duration, 0, 1)
//        (0 = animation not started, 1 = animation finished)
//   3. make it smooth (slow start, slow end) with the "smoothstep" curve
//        smooth = p * p * (3 - 2p)
//   4. use that number to compute WHERE the part should be right now
//        position = startPosition + direction * distance * smooth
//   5. draw the part at that position using a model matrix
//        (glm::translate / glm::rotate / glm::scale)
//
// Because the position is recomputed from time every frame, the
// animation runs at the same speed on a fast or slow computer.
//
// Each animation (boosters, fairing, stages, satellite) just needs two
// global variables: "has it started?" and "when did it start?".
// =====================================================================

const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 800;

// Orbit camera: yaw = angle around the rocket (left/right),
// pitch = angle up/down, distance = how far away the camera is.
float cameraYaw = 35.0f;
float cameraPitch = 18.0f;
float cameraDistance = 18.0f;

// For every animation we store:
//   xxxStarted   -> false until the key is pressed
//   xxxStartTime -> the clock time (glfwGetTime()) at the moment of the key press
// Later, (now - startTime) tells us how far into the animation we are.

// Booster separation animation.
bool boosterSeparationStarted = false;
double boosterSeparationStartTime = 0.0;
// Payload fairing separation.
bool fairingSeparationStarted = false;
double fairingSeparationStartTime = 0.0;

// Stage separation: S starts a timed sequence for stages 1, 2, and 3.
bool stageSeparationStarted = false;
double stageSeparationStartTime = 0.0;

// Satellite deployment: D releases Mangalyaan from the upper stage.
bool satelliteDeploymentStarted = false;
double satelliteDeploymentStartTime = 0.0;

// readFile: opens a text file (a shader .glsl file) and returns its
// whole content as one string. Not animation related, just setup.
string readFile(const string &path)
{
    ifstream file(path);
    if (!file)
        throw runtime_error("Cannot open shader file: " + path);

    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// compileShader: gives the shader source code to the GPU driver and
// compiles it. Prints the error log and throws if there is a mistake.
GLuint compileShader(GLenum type, const string &source)
{
    GLuint shader = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        cerr << "Shader compilation error:\n"
             << log << '\n';
        glDeleteShader(shader);
        throw runtime_error("Shader compilation failed");
    }
    return shader;
}

// createProgram: compiles a vertex shader + fragment shader and links
// them into one "program" that we use when drawing.
GLuint createProgram(const string &vertexPath, const string &fragmentPath)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, readFile(vertexPath));
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, readFile(fragmentPath));

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!success)
    {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        cerr << "Shader link error:\n"
             << log << '\n';
        glDeleteProgram(program);
        throw runtime_error("Shader linking failed");
    }
    return program;
}

// setMatrices: sends the three matrices to the shader.
//   model      -> WHERE the object is, how it is rotated and how big it is.
//                 *** This is the matrix that animation changes. ***
//   view       -> where the camera is and where it looks.
//   projection -> perspective (far things look smaller).
// The shader then computes: screenPos = projection * view * model * vertex
void setMatrices(GLuint program, const glm::mat4 &model,
                 const glm::mat4 &view, const glm::mat4 &projection)
{
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "model"),
                       1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(program, "view"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"),
                       1, GL_FALSE, glm::value_ptr(projection));
}

// drawCylinder: draws one cylinder (or cone) at "position" with size "scale".
// To ANIMATE a part we simply call this every frame with a different
// position (e.g. position.y - drop moves the part down).
//   glm::mat4(1.0f)      -> identity matrix = "no change at all"
//   glm::translate(m, p) -> move by p
//   glm::scale(m, s)     -> stretch by s along x, y, z
// Order matters: the last one written (scale) is applied to the
// vertices first, so the object is resized first and then moved.
void drawCylinder(Cylinder &cylinder, GLuint program,
                  const glm::mat4 &view, const glm::mat4 &projection,
                  const glm::vec3 &position, const glm::vec3 &scale,
                  const glm::vec3 &color)
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, scale);

    setMatrices(program, model, view, projection);
    glUniform3f(glGetUniformLocation(program, "lightPos"), 5.0f, 8.0f, 6.0f);
    glUniform3fv(glGetUniformLocation(program, "objectColor"), 1,
                 glm::value_ptr(color));
    cylinder.draw();
}

// drawRotatedCylinder: same as drawCylinder but also rotates the object.
//   glm::rotate(m, angleInRadians, axis) -> spin around "axis".
//   glm::radians(deg) converts degrees to radians (GLM wants radians).
// Final effect on a vertex: scale -> rotate -> move to position.
// (Currently not called anywhere, kept as a helper.)
void drawRotatedCylinder(
    Cylinder &cylinder,
    GLuint program,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    const glm::vec3 &position,
    const glm::vec3 &scale,
    const glm::vec3 &rotationAxis,
    float rotationAngle,
    const glm::vec3 &color)
{
    glm::mat4 model(1.0f);

    model = glm::translate(model, position);

    model = glm::rotate(
        model,
        glm::radians(rotationAngle),
        rotationAxis);

    model = glm::scale(model, scale);

    setMatrices(program, model, view, projection);

    glUniform3f(
        glGetUniformLocation(program, "lightPos"),
        5.0f, 8.0f, 6.0f);

    glUniform3fv(
        glGetUniformLocation(program, "objectColor"),
        1,
        glm::value_ptr(color));

    cylinder.draw();
}

// drawRocket: draws the WHOLE vehicle once. Called every frame from main().
// This is where all four animations of model 2 are calculated:
//   - stage separation   (S key) -> stages slide down one after another
//   - fairing separation (F key) -> two nose-cone halves slide apart
//   - booster separation (B key) -> 4 boosters move outward and down
//   - satellite deploy   (D key) -> Mangalyaan slides sideways and grows
// Each one follows the same recipe: time -> progress (0..1) ->
// smooth progress -> offset -> draw at (original position + offset).
void drawRocket(Cylinder &cylinder,
                Cylinder &cone,
                Fairing &fairingHalfA,
                Fairing &fairingHalfB,
                Cube &cube,
                GLuint program,
                const glm::mat4 &view,
                const glm::mat4 &projection)
{
    // PSLV-C25 inspired proportions. Model units are illustrative, not exact scale.
    // Base is y=0; vehicle is stacked vertically along +Y.

    const glm::vec3 white(0.88f, 0.88f, 0.84f);
    const glm::vec3 orange(0.90f, 0.30f, 0.08f);
    const glm::vec3 dark(0.22f, 0.24f, 0.28f);
    const glm::vec3 black(0.08f, 0.09f, 0.11f);
    const glm::vec3 gold(0.82f, 0.63f, 0.18f);

    // Helper to draw a coloured cube at any position and size.
    auto drawBox = [&](const glm::vec3 &position,
                       const glm::vec3 &size,
                       const glm::vec3 &color)
    {
        glm::mat4 model(1.0f);

        // Move the cube to its position.
        model = glm::translate(model, position);

        // Resize the cube.
        model = glm::scale(model, size);

        // Set transformation matrices.
        setMatrices(program, model, view, projection);

        // Set lighting and colour.
        glUniform3f(
            glGetUniformLocation(program, "lightPos"),
            5.0f, 8.0f, 6.0f);

        glUniform3fv(
            glGetUniformLocation(program, "objectColor"),
            1,
            glm::value_ptr(color));

        // Draw the cube.
        cube.draw();
    };

    //   ROCKET STAGES  
    // Press S to start a simple sequential stage-separation animation.
    // Each spent lower stage moves downward and fades out of the scene

    // stageElapsed = seconds since S was pressed (0 if not pressed yet).
    // glfwGetTime() = seconds since the program started (a stopwatch).
    float stageElapsed = stageSeparationStarted
        ? static_cast<float>(glfwGetTime() - stageSeparationStartTime)
        : 0.0f;

    // stageDrop(delay): how far DOWN (in model units) a stage has fallen.
    // "delay" lets stages fall one after another:
    //   stage 1 starts at 0 s, stage 2 at 1.8 s, stage 3 at 3.6 s.
    // Steps:
    //   (stageElapsed - delay) / 1.8  -> 0 at start, 1 after 1.8 seconds
    //   glm::clamp(..., 0, 1)         -> never below 0 or above 1, so the
    //                                    stage stops after falling fully
    //   p*p*(3-2p)                    -> smoothstep: slow-fast-slow motion
    //   7 * p                         -> total fall distance is 7 units
    auto stageDrop = [&](float delay)
    {
        if (!stageSeparationStarted || stageElapsed <= delay)
            return 0.0f;

        float p = glm::clamp((stageElapsed - delay) / 1.8f, 0.0f, 1.0f);
        p = p * p * (3.0f - 2.0f * p);
        return 7.0f * p;
    };

    // For each stage: subtract the drop from its Y position (moves it
    // down). When it has nearly fallen the full 7 units we stop drawing
    // it, so it "disappears" from the scene.

    // Stage 1 (white lower body) and its interstage ring.
    // It starts moving after the S sequence begins.
    float drop1 = stageDrop(0.0f);
    if (drop1 < 6.95f)
    {
        drawCylinder(cylinder, program, view, projection,
                     glm::vec3(0.0f, 1.75f - drop1, 0.0f),
                     glm::vec3(1.45f, 3.5f, 1.45f), white);
        drawCylinder(cylinder, program, view, projection,
                     glm::vec3(0.0f, 3.58f - drop1, 0.0f),
                     glm::vec3(1.48f, 0.18f, 1.48f), dark);
        drawCylinder(cylinder, program, view, projection,
                     glm::vec3(0.0f, 0.95f - drop1, 0.0f),
                     glm::vec3(1.46f, 0.12f, 1.46f), orange);
    }

    // Stage 2 (orange body) follows after stage 1 has begun separating.
    float drop2 = stageDrop(1.8f);
    if (drop2 < 6.95f)
    {
        drawCylinder(cylinder, program, view, projection,
                     glm::vec3(0.0f, 4.65f - drop2, 0.0f),
                     glm::vec3(1.12f, 2.0f, 1.12f), orange);
        drawCylinder(cylinder, program, view, projection,
                     glm::vec3(0.0f, 5.72f - drop2, 0.0f),
                     glm::vec3(1.16f, 0.16f, 1.16f), dark);
        drawCylinder(cylinder, program, view, projection,
                     glm::vec3(0.0f, 5.35f - drop2, 0.0f),
                     glm::vec3(1.13f, 0.10f, 1.13f), orange);
    }

    // Stage 3 (white upper body) follows stage 2.
    float drop3 = stageDrop(3.6f);
    if (drop3 < 6.95f)
    {
        drawCylinder(cylinder, program, view, projection,
                     glm::vec3(0.0f, 6.65f - drop3, 0.0f),
                     glm::vec3(0.88f, 1.75f, 0.88f), white);
    }

    // Upper/final stage (PS4) and payload adapter always stay in place;
    // Mangalyaan moves away from them when D is pressed.
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 7.78f, 0.0f),
                 glm::vec3(0.72f, 0.55f, 0.72f), dark);
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 8.18f, 0.0f),
                 glm::vec3(0.62f, 0.28f, 0.62f), gold);

    //   PAYLOAD FAIRING SEPARATION
    // Same recipe: progress goes 0 -> 1 in 2 seconds after F is pressed,
    // gets smoothed, and the two halves are pushed apart along Z
    // (half A towards +Z, half B towards -Z) by up to 1.55 units.
    float fairingProgress = 0.0f;
    if (fairingSeparationStarted)
    {
        double elapsed = glfwGetTime() - fairingSeparationStartTime;
        fairingProgress = glm::clamp(
            static_cast<float>(elapsed / 2.0), 0.0f, 1.0f);
    }

    float fairingSmooth =
        fairingProgress * fairingProgress * (3.0f - 2.0f * fairingProgress);

    const glm::vec3 fairingPosition(0.0f, 9.05f, 0.0f);
    const glm::vec3 fairingScale(1.22f, 1.55f, 1.22f);

    // Fairing halves are split along the Z axis in Fairing.cpp: half A
    // (index 0) is the +Z half and half B (index 1) is the -Z half, so each
    // one moves outward on its own side.
    float separationDistance = 1.55f * fairingSmooth;
    glm::vec3 fairingOffsetA(0.0f, 0.0f,  separationDistance);
    glm::vec3 fairingOffsetB(0.0f, 0.0f, -separationDistance);

    glm::mat4 modelA(1.0f);
    modelA = glm::translate(modelA, fairingPosition + fairingOffsetA);
    modelA = glm::scale(modelA, fairingScale);
    setMatrices(program, modelA, view, projection);
    glUniform3f(glGetUniformLocation(program, "lightPos"), 5.0f, 8.0f, 6.0f);
    glUniform3f(glGetUniformLocation(program, "objectColor"), 0.88f, 0.88f, 0.84f);
    fairingHalfA.draw();

    glm::mat4 modelB(1.0f);
    modelB = glm::translate(modelB, fairingPosition + fairingOffsetB);
    modelB = glm::scale(modelB, fairingScale);
    setMatrices(program, modelB, view, projection);
    glUniform3f(glGetUniformLocation(program, "lightPos"), 5.0f, 8.0f, 6.0f);
    glUniform3f(glGetUniformLocation(program, "objectColor"), 0.88f, 0.88f, 0.84f);
    fairingHalfB.draw();

    //   BOOSTER SEPARATION  

    // Calculate how far the animation has progressed.
    // Separation takes 2 seconds.
    float separationProgress = 0.0f;

    if (boosterSeparationStarted)
    {
        double elapsed =
            glfwGetTime() - boosterSeparationStartTime;

        separationProgress =
            glm::clamp(static_cast<float>(elapsed / 2.0), 0.0f, 1.0f);
    }

    // Smooth the animation so it starts and ends gently.
    float t = separationProgress;

    float smoothProgress =
        t * t * (3.0f - 2.0f * t);

    // Four strap-on boosters.
    // They sit on a circle around the rocket at angles 45, 135, 225, 315 deg.
    // (cos(a), sin(a)) is the point on that circle, i.e. the direction
    // pointing straight AWAY from the rocket's centre. Moving along it
    // makes each booster fly off on its own side.
    for (int i = 0; i < 4; ++i)
    {
        float a = glm::radians(45.0f + 90.0f * i);

        // Original booster position.
        glm::vec3 originalPosition(
            0.92f * cos(a),
            1.85f,
            0.92f * sin(a));

        // Direction away from the rocket.
        glm::vec3 outwardDirection(
            cos(a),
            0.0f,
            sin(a));

        // Move each booster outward and slightly downward.
        glm::vec3 separationOffset =
            outwardDirection * (2.2f * smoothProgress) + glm::vec3(0.0f, -0.4f * smoothProgress, 0.0f);

        glm::vec3 p = originalPosition + separationOffset;

        // Draw the booster body.
        drawCylinder(
            cylinder, program, view, projection,
            p,
            glm::vec3(0.38f, 3.1f, 0.38f),
            white);

        // Draw the booster nose tip.
        glm::vec3 tipPos =
            p + glm::vec3(0.0f, 1.65f, 0.0f);

        drawCylinder(
            cone, program, view, projection,
            tipPos,
            glm::vec3(0.38f, 0.48f, 0.38f),
            orange);
    }

    //   MANGALYAAN SPACECRAFT  
    // Before F: hidden inside the closed fairing.
    // After F: visible but still attached to the upper stage.
    // Press D: deploy it away from the rocket and expand it to full size.

    float deployProgress = 0.0f;
    if (satelliteDeploymentStarted)
    {
        float elapsed = static_cast<float>(
            glfwGetTime() - satelliteDeploymentStartTime);
        deployProgress = glm::clamp(elapsed / 2.5f, 0.0f, 1.0f);
    }
    float deploySmooth =
        deployProgress * deployProgress * (3.0f - 2.0f * deployProgress);

    // Two things are animated with deploySmooth (0 -> 1 in 2.5 s):
    //   satelliteScale : 0.28 -> 1.0   (it "unfolds" / grows)
    //   deployedOffset : moves 3.8 units to +X and 0.25 up (drifts away)
    // This is "linear interpolation": start + (end - start) * t.
    // Every satellite piece is placed relative to spacecraftCenter and
    // multiplied by satelliteScale, so the whole satellite grows together.
    if (fairingSeparationStarted)
    {
        // Start compact enough to fit inside the payload fairing.
        // During D, scale up and move to the side to show deployment.
        const glm::vec3 stowedCenter(0.0f, 8.75f, 0.0f);
        const float satelliteScale = 0.28f + 0.72f * deploySmooth;
        const glm::vec3 deployedOffset(3.8f * deploySmooth, 0.25f * deploySmooth, 0.0f);
        glm::vec3 spacecraftCenter = stowedCenter + deployedOffset;

        auto drawSatelliteBox = [&](const glm::vec3 &localPosition,
                                    const glm::vec3 &localSize,
                                    const glm::vec3 &color)
        {
            drawBox(spacecraftCenter + localPosition * satelliteScale,
                    localSize * satelliteScale, color);
        };

        // Main satellite body.
        drawSatelliteBox(glm::vec3(0.0f), glm::vec3(0.85f, 0.65f, 0.65f),
                         glm::vec3(0.82f, 0.62f, 0.22f));

        // Dark panel mounting sections.
        for (int side : {-1, 1})
        {
            drawSatelliteBox(glm::vec3(0.0f, 0.0f, side * 0.55f),
                             glm::vec3(0.16f, 0.12f, 0.35f),
                             glm::vec3(0.35f, 0.35f, 0.38f));
        }

        // Solar panels and their grid lines.
        const glm::vec3 panelColor(0.035f, 0.12f, 0.34f);
        const glm::vec3 gridColor(0.32f, 0.55f, 0.78f);

        for (int side : {-1, 1})
        {
            glm::vec3 panelCenter(0.0f, 0.0f, side * 1.45f);
            drawSatelliteBox(panelCenter, glm::vec3(0.95f, 0.055f, 1.45f), panelColor);

            for (int i = -2; i <= 2; ++i)
            {
                float x = i * 0.16f;
                drawSatelliteBox(panelCenter + glm::vec3(x, 0.032f, 0.0f),
                                 glm::vec3(0.012f, 0.012f, 1.40f), gridColor);
            }

            for (int i = -3; i <= 3; ++i)
            {
                float z = i * 0.19f;
                drawSatelliteBox(panelCenter + glm::vec3(0.0f, 0.033f, z),
                                 glm::vec3(0.92f, 0.012f, 0.012f), gridColor);
            }
        }

        // Antenna mast and dish. Scale their positions and dimensions too.
        drawCylinder(
            cylinder, program, view, projection,
            spacecraftCenter + glm::vec3(0.0f, 0.48f * satelliteScale, 0.0f),
            glm::vec3(0.045f, 0.45f, 0.045f) * satelliteScale,
            glm::vec3(0.75f, 0.75f, 0.72f));

        drawCylinder(
            cone, program, view, projection,
            spacecraftCenter + glm::vec3(0.0f, 0.73f * satelliteScale, 0.0f),
            glm::vec3(0.15f, 0.10f, 0.15f) * satelliteScale,
            glm::vec3(0.78f, 0.78f, 0.74f));
    }
}

// processInput: called once per frame to read the keyboard.
//   - Arrow keys / [ ] change the camera angles and distance.
//     glm::min / glm::max keep them inside limits (e.g. pitch <= 80).
//   - B, F, S, D START an animation. Starting just means:
//       xxxStarted = true; xxxStartTime = glfwGetTime();
//     The actual movement is then computed in drawRocket().
//   - "xWasDown" remembers if the key was already held last frame, so
//     holding a key down only triggers once (edge detection).
void processInput(GLFWwindow *window)
{
    const float angleStep = 1.2f;
    const float zoomStep = 0.25f;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        cameraYaw -= angleStep;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        cameraYaw += angleStep;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cameraPitch = glm::min(cameraPitch + angleStep, 80.0f);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cameraPitch = glm::max(cameraPitch - angleStep, -10.0f);

    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        cameraDistance = glm::max(cameraDistance - zoomStep, 5.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        cameraDistance = glm::min(cameraDistance + zoomStep, 45.0f);

    // Use key-release detection so holding a key does not retrigger it.
    static bool bWasDown = false;
    static bool fWasDown = false;
    static bool sWasDown = false;
    static bool dWasDown = false;

    bool bDown = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    bool fDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    bool sDown = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool dDown = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

    if (bDown && !bWasDown && !boosterSeparationStarted)
    {
        boosterSeparationStarted = true;
        boosterSeparationStartTime = glfwGetTime();
    }

    if (fDown && !fWasDown && !fairingSeparationStarted)
    {
        fairingSeparationStarted = true;
        fairingSeparationStartTime = glfwGetTime();
    }

    // Stage separation. The boosters are mounted on stage 1, so they are
    // released too if B has not been pressed yet.
    if (sDown && !sWasDown && !stageSeparationStarted)
    {
        stageSeparationStarted = true;
        stageSeparationStartTime = glfwGetTime();
        if (!boosterSeparationStarted)
        {
            boosterSeparationStarted = true;
            boosterSeparationStartTime = glfwGetTime();
        }
    }

    // Deploy Mangalyaan. The fairing is opened too if F has not been pressed.
    if (dDown && !dWasDown && !satelliteDeploymentStarted)
    {
        satelliteDeploymentStarted = true;
        satelliteDeploymentStartTime = glfwGetTime();
        if (!fairingSeparationStarted)
        {
            fairingSeparationStarted = true;
            fairingSeparationStartTime = glfwGetTime();
        }
    }

    bWasDown = bDown;
    fWasDown = fDown;
    sWasDown = sDown;
    dWasDown = dDown;
}

// main: creates the window, loads shaders and meshes, then runs the
// render loop. Each loop iteration = one frame:
//   read keys -> clear screen -> compute camera -> drawRocket() -> show.
// Because drawRocket() recalculates every part's position from the
// current time, the picture changes a little every frame = animation.
int main()
{
    if (!glfwInit())
    {
        cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(
        WINDOW_WIDTH, WINDOW_HEIGHT, "a2-model-2 - PSLV-C25 and Mangalyaan",
        nullptr, nullptr);

    if (!window)
    {
        cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;

    if (glewInit() != GLEW_OK)
    {
        cerr << "Failed to initialize GLEW\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    glGetError();

    glEnable(GL_DEPTH_TEST);

    GLuint program = 0;
    try
    {
        program = createProgram(
            "shaders/rocket_vertex.glsl",
            "shaders/rocket_fragment.glsl");

        Cylinder cylinder(64, false);
        Cylinder cone(64, true);
        Cube cube;

        // Two halves of the payload fairing.
        Fairing fairingHalfA(0, 36);
        Fairing fairingHalfB(1, 36);

        cout << "a2-model-2 controls:\n"
            << "  Arrow keys: orbit camera\n"
            << "  [ and ]: zoom in/out\n"
            << "  B: separate boosters\n"
            << "  F: separate payload fairing / reveal Mangalyaan\n"
            << "  S: start sequential stage separation\n"
            << "  D: deploy Mangalyaan\n"
            << "  Esc: exit\n";

        while (!glfwWindowShouldClose(window))
        {
            processInput(window);

            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            if (height == 0)
                height = 1;
            glViewport(0, 0, width, height);

            glClearColor(0.025f, 0.035f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            float yaw = glm::radians(cameraYaw);
            float pitch = glm::radians(cameraPitch);

            // Spherical -> Cartesian: turn (yaw, pitch, distance) into an
            // x,y,z camera position on a sphere around "target".
            // glm::lookAt(eye, target, up) builds the view matrix that
            // points the camera from "eye" at "target".
            glm::vec3 target(0.8f, 4.7f, 0.0f);
            glm::vec3 cameraPosition(
                target.x + cameraDistance * cos(pitch) * sin(yaw),
                target.y + cameraDistance * sin(pitch),
                target.z + cameraDistance * cos(pitch) * cos(yaw));

            glm::mat4 view = glm::lookAt(
                cameraPosition, target, glm::vec3(0.0f, 1.0f, 0.0f));

            glm::mat4 projection = glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(width) / static_cast<float>(height),
                0.1f, 100.0f);

            drawRocket(
                cylinder,
                cone,
                fairingHalfA,
                fairingHalfB,
                cube,
                program,
                view,
                projection);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << '\n';
    }

    if (program)
        glDeleteProgram(program);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
