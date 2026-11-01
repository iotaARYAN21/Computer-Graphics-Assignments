#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../models/Cylinder.h"
#include "../models/Cube.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 800;

float cameraYaw = 35.0f;
float cameraPitch = 18.0f;
float cameraDistance = 18.0f;

// Booster separation animation.
bool boosterSeparationStarted = false;
double boosterSeparationStartTime = 0.0;
// Payload fairing separation.
bool fairingSeparationStarted = false;
double fairingSeparationStartTime = 0.0;

string readFile(const string &path)
{
    ifstream file(path);
    if (!file)
        throw runtime_error("Cannot open shader file: " + path);

    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

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

void drawRocket(Cylinder &cylinder,
                Cylinder &cone,
                Cylinder &fairingHalfA,
                Cylinder &fairingHalfB,
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

    // First stage: largest lower cylinder.
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 1.75f, 0.0f),
                 glm::vec3(1.45f, 3.5f, 1.45f), white);

    // Interstage ring above first stage.
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 3.58f, 0.0f),
                 glm::vec3(1.48f, 0.18f, 1.48f), dark);

    // Second stage.
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 4.65f, 0.0f),
                 glm::vec3(1.12f, 2.0f, 1.12f), orange);

    // Interstage ring.
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 5.72f, 0.0f),
                 glm::vec3(1.16f, 0.16f, 1.16f), dark);

    // Third stage.
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 6.65f, 0.0f),
                 glm::vec3(0.88f, 1.75f, 0.88f), white);

    // Fourth stage (PS4).
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 7.78f, 0.0f),
                 glm::vec3(0.72f, 0.55f, 0.72f), dark);

    // Payload adapter.
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 8.18f, 0.0f),
                 glm::vec3(0.62f, 0.28f, 0.62f), gold);

    // Payload fairing: simple pointed cone above the fourth stage.
    // drawCylinder(cone, program, view, projection,
    //              glm::vec3(0.0f, 9.05f, 0.0f),
    //              glm::vec3(1.22f, 1.55f, 1.22f), white);

    // ================= PAYLOAD FAIRING SEPARATION =================

    // The fairing starts separating when F is pressed.
    // Animation duration: 2 seconds.
    float fairingProgress = 0.0f;

    if (fairingSeparationStarted)
    {
        double elapsed =
            glfwGetTime() - fairingSeparationStartTime;

        fairingProgress =
            glm::clamp(
                static_cast<float>(elapsed / 2.0),
                0.0f,
                1.0f);
    }

    // Smooth the movement.
    float fairingSmooth =
        fairingProgress * fairingProgress *
        (3.0f - 2.0f * fairingProgress);

    // Original centre of the fairing.
    glm::vec3 fairingPosition(0.0f, 9.05f, 0.0f);

    // Original fairing dimensions.
    glm::vec3 fairingScale(1.22f, 1.55f, 1.22f);

    // Move the two halves in opposite directions.
    float separationDistance = 1.5f * fairingSmooth;

    glm::vec3 fairingOffsetA(
        -separationDistance, 0.0f, 0.0f);

    glm::vec3 fairingOffsetB(
        separationDistance, 0.0f, 0.0f);

    // Draw the first half.
    drawRotatedCylinder(
        fairingHalfA,
        program,
        view,
        projection,
        fairingPosition + fairingOffsetA,
        fairingScale,
        glm::vec3(0.0f, 1.0f, 0.0f),
        0.0f,
        white);

    // Draw the second half, rotated 180 degrees.
    drawRotatedCylinder(
        fairingHalfB,
        program,
        view,
        projection,
        fairingPosition + fairingOffsetB,
        fairingScale,
        glm::vec3(0.0f, 1.0f, 0.0f),
        180.0f,
        white);


    // Two small orange bands to make stage boundaries visible.
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 0.95f, 0.0f),
                 glm::vec3(1.46f, 0.12f, 1.46f), orange);
    drawCylinder(cylinder, program, view, projection,
                 glm::vec3(0.0f, 5.35f, 0.0f),
                 glm::vec3(1.13f, 0.10f, 1.13f), orange);

    // ================= BOOSTER SEPARATION =================

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

    // Four simplified solid strap-on boosters around first stage.
    // for (int i = 0; i < 4; ++i)
    // {
    //     float a = glm::radians(45.0f + 90.0f * i);
    //     glm::vec3 p(0.92f * cos(a), 1.85f, 0.92f * sin(a));
    //     drawCylinder(cylinder, program, view, projection,
    //                  p, glm::vec3(0.38f, 3.1f, 0.38f), white);

    //     // Booster nose tips, narrow cones.
    //     glm::vec3 tipPos = p + glm::vec3(0.0f, 1.65f, 0.0f);
    //     drawCylinder(cone, program, view, projection,
    //                  tipPos, glm::vec3(0.38f, 0.48f, 0.38f), orange);
    // }

    // Mangalyaan orbiter shown beside the rocket so its components are visible.
    // Simplified box bus and two solar-panel wings.
    // ================= MANGLAYAAN SPACECRAFT =================

    // Position of the spacecraft beside the rocket.
    glm::vec3 spacecraftCenter(2.8f, 8.0f, 0.0f);

    // 1. Main satellite body: gold thermal insulation.
    drawBox(
        spacecraftCenter,
        glm::vec3(0.85f, 0.65f, 0.65f),
        glm::vec3(0.82f, 0.62f, 0.22f));

    // 2. Dark mounting sections between body and solar panels.
    for (int side : {-1, 1})
    {
        drawBox(
            spacecraftCenter + glm::vec3(0.0f, 0.0f, side * 0.55f),
            glm::vec3(0.16f, 0.12f, 0.35f),
            glm::vec3(0.35f, 0.35f, 0.38f));
    }

    // 3. Solar panels and their grid lines.
    glm::vec3 panelColor(0.035f, 0.12f, 0.34f);
    glm::vec3 gridColor(0.32f, 0.55f, 0.78f);

    for (int side : {-1, 1})
    {
        // Centre of this solar panel.
        glm::vec3 panelCenter =
            spacecraftCenter + glm::vec3(0.0f, 0.0f, side * 1.45f);

        // Main rectangular solar panel.
        drawBox(
            panelCenter,
            glm::vec3(0.95f, 0.055f, 1.45f),
            panelColor);

        // Grid lines running along the length of the panel.
        for (int i = -2; i <= 2; ++i)
        {
            float x = i * 0.16f;

            drawBox(
                panelCenter + glm::vec3(x, 0.032f, 0.0f),
                glm::vec3(0.012f, 0.012f, 1.40f),
                gridColor);
        }

        // Cross-lines running across the panel.
        for (int i = -3; i <= 3; ++i)
        {
            float z = i * 0.19f;

            drawBox(
                panelCenter + glm::vec3(0.0f, 0.033f, z),
                glm::vec3(0.92f, 0.012f, 0.012f),
                gridColor);
        }
    }

    // 4. Antenna mast above the spacecraft.
    drawCylinder(
        cylinder,
        program,
        view,
        projection,
        spacecraftCenter + glm::vec3(0.0f, 0.48f, 0.0f),
        glm::vec3(0.045f, 0.45f, 0.045f),
        glm::vec3(0.75f, 0.75f, 0.72f));

    // 5. Small dish-like antenna element.
    drawCylinder(
        cone,
        program,
        view,
        projection,
        spacecraftCenter + glm::vec3(0.0f, 0.73f, 0.0f),
        glm::vec3(0.15f, 0.10f, 0.15f),
        glm::vec3(0.78f, 0.78f, 0.74f));
}

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

    // Start booster separation when B is pressed.
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !boosterSeparationStarted)
    {
        boosterSeparationStarted = true;
        boosterSeparationStartTime = glfwGetTime();
    }
    // Start payload fairing separation.
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !fairingSeparationStarted)
    {
        fairingSeparationStarted = true;
        fairingSeparationStartTime = glfwGetTime();
    }
}

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
        Cylinder fairingHalfA(36, true, true);
        Cylinder fairingHalfB(36, true, true);

        cout << "a2-model-2 controls:\n"
            << "  Arrow keys: orbit camera\n"
            << "  [ and ]: zoom in/out\n"
            << "  B: separate boosters\n"
            << "  F: separate payload fairing\n"
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

            // drawRocket(cylinder, cone, cube, program, view, projection);
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
