#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#include "../models/Cube.h"
#include "../models/Plane.h"

#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

const unsigned int WINDOW_WIDTH = 1000;
const unsigned int WINDOW_HEIGHT = 700;

float cameraYaw = 45.0f;
float cameraPitch = 30.0f;
float cameraDistance = 18.0f;

GLuint compileShader(GLenum type, const string& source)
{
    GLuint shader = glCreateShader(type);
    const char* sourceText = source.c_str();
    glShaderSource(shader, 1, &sourceText, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        cerr << "Shader compilation failed:\n" << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

string readFile(const string& path)
{
    ifstream file(path);
    if (!file)
    {
        throw runtime_error("Could not open shader file: " + path);
    }
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint createProgram(const string& vertexPath, const string& fragmentPath)
{
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, readFile(vertexPath));
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, readFile(fragmentPath));

    if (!vertexShader || !fragmentShader)
    {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        throw runtime_error("Could not compile shader program.");
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        cerr << "Shader linking failed:\n" << log << '\n';
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!program)
        throw runtime_error("Could not link shader program.");

    return program;
}

void setMatrices(GLuint program, const glm::mat4& model,
                 const glm::mat4& view, const glm::mat4& projection)
{
    glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE,
                       glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE,
                       glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE,
                       glm::value_ptr(projection));
}

void drawCube(Cube& cube, GLuint program, const glm::mat4& view,
              const glm::mat4& projection, const glm::vec3& position,
              const glm::vec3& scale, const glm::vec3& color)
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, scale);

    glUseProgram(program);
    setMatrices(program, model, view, projection);

    glUniform3f(glGetUniformLocation(program, "lightPos"), 4.0f, 8.0f, 5.0f);
    glUniform3fv(glGetUniformLocation(program, "objectColor"), 1,
                 glm::value_ptr(color));
    cube.draw();
}

void drawBeam(
    Cube& cube,
    GLuint program,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec3 start,
    glm::vec3 end,
    float thickness,
    const glm::vec3& color)
{
    glm::vec3 direction = end - start;
    float length = glm::length(direction);

    if (length < 0.0001f)
        return;

    glm::vec3 midpoint = (start + end) * 0.5f;

    // Rotate the cube's Y-axis to point along the beam.
    glm::vec3 yAxis(0.0f, 1.0f, 0.0f);
    glm::vec3 dir = direction / length;
    glm::quat rotation = glm::rotation(yAxis, dir);

    glm::mat4 model(1.0f);
    model = glm::translate(model, midpoint);
    model *= glm::mat4_cast(rotation);

    // Cube is centred at the origin and has actual dimensions 1x1x1
    // (from -0.5 to 0.5 in each dimension), so we scale by full length.
    model = glm::scale(
        model,
        glm::vec3(thickness, length, thickness));

    glUseProgram(program);
    setMatrices(program, model, view, projection);
    glUniform3f(
        glGetUniformLocation(program, "lightPos"),
        4.0f, 8.0f, 5.0f);
    glUniform3fv(
        glGetUniformLocation(program, "objectColor"),
        1, glm::value_ptr(color));
    cube.draw();
}

void processInput(GLFWwindow* window)
{
    const float angleStep = 1.5f;
    const float zoomStep = 0.25f;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  cameraYaw -= angleStep;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) cameraYaw += angleStep;

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cameraPitch = glm::min(cameraPitch + angleStep, 80.0f);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cameraPitch = glm::max(cameraPitch - angleStep, 5.0f);

    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        cameraDistance = glm::max(cameraDistance - zoomStep, 4.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        cameraDistance = glm::min(cameraDistance + zoomStep, 40.0f);
}

int main()
{
    if (!glfwInit())
    {
        cerr << "Failed to initialize GLFW.\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        WINDOW_WIDTH, WINDOW_HEIGHT, "A2 Model 1 - Launch Site", nullptr, nullptr);
    if (!window)
    {
        cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        cerr << "Failed to initialize GLEW.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    // GLEW can leave an OpenGL error behind when initialized in a core context.
    glGetError();

    glEnable(GL_DEPTH_TEST);

    try
    {
        // Reuse your existing cube shaders; do not replace them.
        GLuint cubeProgram = createProgram(
            "shaders/cube_vertex.glsl", "shaders/cube_fragment.glsl");
        GLuint groundProgram = createProgram(
            "shaders/ground_vertex.glsl", "shaders/ground_fragment.glsl");

        Cube cube;
        Plane ground;

        cout << "Controls:\n"
             << "  Arrow keys: orbit camera\n"
             << "  [ and ]: zoom in/out\n"
             << "  Esc: exit\n";

        while (!glfwWindowShouldClose(window))
        {
            processInput(window);

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            if (height == 0) height = 1;
            glViewport(0, 0, width, height);

            glClearColor(0.48f, 0.68f, 0.88f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            float yaw = glm::radians(cameraYaw);
            float pitch = glm::radians(cameraPitch);
            glm::vec3 target(0.0f, 0.35f, 0.0f);
            glm::vec3 cameraPosition(
                cameraDistance * cos(pitch) * sin(yaw),
                cameraDistance * sin(pitch),
                cameraDistance * cos(pitch) * cos(yaw));

            glm::mat4 view = glm::lookAt(
                cameraPosition, target, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 projection = glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(width) / static_cast<float>(height),
                0.1f, 100.0f);

            // Large green ground plane.
            glUseProgram(groundProgram);
            glm::mat4 groundModel(1.0f);
            setMatrices(groundProgram, groundModel, view, projection);
            ground.draw();

            // Main concrete launch platform.
            drawCube(cube, cubeProgram, view, projection,
                     glm::vec3(0.0f, 0.18f, 0.0f),
                     glm::vec3(6.0f, 0.36f, 5.0f),
                     glm::vec3(0.48f, 0.49f, 0.52f));
            // Raised central launch pedestal.
            drawCube(cube, cubeProgram, view, projection,
                     glm::vec3(0.0f, 0.52f, 0.0f),
                     glm::vec3(2.0f, 0.35f, 2.0f),
                     glm::vec3(0.68f, 0.69f, 0.71f));

              
            // Simplified launch-site towers
              
            const glm::vec3 towerColor(1.0f, 0.65f, 0.0f);
            glm::vec3 towerPositions[] = {
                glm::vec3(-4.5f, 2.0f, -3.5f),
                glm::vec3( 4.5f, 2.0f, -3.5f),
                glm::vec3(-4.5f, 2.0f,  3.5f),
                glm::vec3( 4.5f, 2.0f,  3.5f)
            };
            for (int i = 0; i < 4; i++)
            {
                drawCube(
                    cube,
                    cubeProgram,
                    view,
                    projection,
                    towerPositions[i],
                    glm::vec3(0.35f, 2.0f, 0.35f),
                    towerColor
                );
            }

              
            // Service tower - structurally aligned frame
              
            glm::vec3 steelColor(0.65f, 0.68f, 0.72f);
            // Position and dimensions
            float towerX = -4.2f;
            float towerZ = -1.0f;
            float towerHeight = 5.0f;
            float towerWidth = 2.0f;
            float beamThickness = 0.10f;
            float bottomY = 0.36f; // Matched to the top of the launch platform
            float topY = bottomY + towerHeight;
            
            // Half dimensions for corner positioning
            float halfW = towerWidth * 0.5f;
            float halfT = beamThickness * 0.5f;
            float halfH = towerHeight * 0.5f;

            // Draw a beam parallel to the X axis.
            auto drawXBeam = [&](float y, float z, float thickness)
            {
                drawCube(
                    cube, cubeProgram, view, projection,
                    glm::vec3(towerX, y, z),
                    glm::vec3(towerWidth, thickness, thickness), // Use full width
                    steelColor
                );
            };
            // Draw a beam parallel to the Z axis.
            auto drawZBeam = [&](float x, float y, float thickness)
            {
                drawCube(
                    cube, cubeProgram, view, projection,
                    glm::vec3(x, y, towerZ),
                    glm::vec3(thickness, thickness, towerWidth), // Use full width
                    steelColor
                );
            };
            
            // Four vertical columns.
            for (int xSide = -1; xSide <= 1; xSide += 2)
            {
                for (int zSide = -1; zSide <= 1; zSide += 2)
                {
                    drawCube(
                        cube, cubeProgram, view, projection,
                        glm::vec3(
                            towerX + xSide * (halfW - halfT),
                            bottomY + halfH,
                            towerZ + zSide * (halfW - halfT)
                        ),
                        glm::vec3(beamThickness, towerHeight, beamThickness), 
                        steelColor
                    );
                }
            }
            // Horizontal frames at the bottom, top, and three intermediate levels.
            for (int level = 0; level <= 4; level++)
            {
                float y = bottomY + towerHeight * level / 4.0f;
                // Front and back beams.
                drawXBeam(y, towerZ - halfW, halfT);
                drawXBeam(y, towerZ + halfW, halfT);
                // Left and right beams.
                drawZBeam(towerX - halfW, y, halfT);
                drawZBeam(towerX + halfW, y, halfT);
            }
            // Diagonal braces
            // Each brace is drawn as a rotated cube between two points.
            for (int level = 0; level < 4; level++)
            {
                float y0 = bottomY + towerHeight * level / 4.0f;
                float y1 = bottomY + towerHeight * (level + 1) / 4.0f;

                // Back face diagonals
                glm::vec3 leftBottom(towerX - halfW, y0, towerZ - halfW);
                glm::vec3 rightTop(towerX + halfW, y1, towerZ - halfW);
                glm::vec3 rightBottom(towerX + halfW, y0, towerZ - halfW);
                glm::vec3 leftTop(towerX - halfW, y1, towerZ - halfW);

                drawBeam(cube, cubeProgram, view, projection, leftBottom, rightTop, 0.07f, steelColor);
                drawBeam(cube, cubeProgram, view, projection, rightBottom, leftTop, 0.07f, steelColor);

                // Front face diagonals
                leftBottom = glm::vec3(towerX - halfW, y0, towerZ + halfW);
                rightTop = glm::vec3(towerX + halfW, y1, towerZ + halfW);
                rightBottom = glm::vec3(towerX + halfW, y0, towerZ + halfW);
                leftTop = glm::vec3(towerX - halfW, y1, towerZ + halfW);

                drawBeam(cube, cubeProgram, view, projection, leftBottom, rightTop, 0.07f, steelColor);
                drawBeam(cube, cubeProgram, view, projection, rightBottom, leftTop, 0.07f, steelColor);

                // Left face diagonals
                leftBottom = glm::vec3(towerX - halfW, y0, towerZ - halfW);
                rightTop = glm::vec3(towerX - halfW, y1, towerZ + halfW);
                rightBottom = glm::vec3(towerX - halfW, y0, towerZ + halfW);
                leftTop = glm::vec3(towerX - halfW, y1, towerZ - halfW);

                drawBeam(cube, cubeProgram, view, projection, leftBottom, rightTop, 0.07f, steelColor);
                drawBeam(cube, cubeProgram, view, projection, rightBottom, leftTop, 0.07f, steelColor);

                // Right face diagonals
                leftBottom = glm::vec3(towerX + halfW, y0, towerZ - halfW);
                rightTop = glm::vec3(towerX + halfW, y1, towerZ + halfW);
                rightBottom = glm::vec3(towerX + halfW, y0, towerZ + halfW);
                leftTop = glm::vec3(towerX + halfW, y1, towerZ - halfW);

                drawBeam(cube, cubeProgram, view, projection, leftBottom, rightTop, 0.07f, steelColor);
                drawBeam(cube, cubeProgram, view, projection, rightBottom, leftTop, 0.07f, steelColor);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        glDeleteProgram(cubeProgram);
        glDeleteProgram(groundProgram);
    }
    catch (const exception& error)
    {
        cerr << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}