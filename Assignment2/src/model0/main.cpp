#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

#include "../common/Mesh.h"
#include "../common/Starfield.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

const float PI = 3.14159265359f;

vector<float> vertices;
vector<unsigned int> indices;

string readFile(const string& path)
{
    ifstream file(path);
    if (!file.is_open())
        throw runtime_error("Cannot open shader: " + path);

    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compileShader(GLenum type, const string& source)
{
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        cerr << "Shader compilation error:\n" << log << '\n';
        glDeleteShader(shader);
        throw runtime_error("Shader compilation failed");
    }
    return shader;
}

GLuint createShaderProgram(const string& vertexPath, const string& fragmentPath)
{
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, readFile(vertexPath));
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, readFile(fragmentPath));

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!success)
    {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        cerr << "Shader linking error:\n" << log << '\n';
        glDeleteProgram(program);
        throw runtime_error("Shader linking failed");
    }
    return program;
}

GLuint loadEarthTexture(const char* path)
{
    int width = 0, height = 0, channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* image = stbi_load(path, &width, &height, &channels, 0);

    if (!image)
        throw runtime_error(string("Cannot load Earth texture: ") + path +
                            "\n" + stbi_failure_reason());

    GLenum format;
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;
    else
    {
        stbi_image_free(image);
        throw runtime_error("Unsupported Earth texture format");
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                 format, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(image);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    cout << "Loaded Earth texture: " << width << " x " << height
         << ", channels: " << channels << '\n';
    return texture;
}

void setMatrixUniforms(GLuint program,
                       const glm::mat4& model,
                       const glm::mat4& view,
                       const glm::mat4& projection)
{
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "model"),
                       1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(program, "view"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"),
                       1, GL_FALSE, glm::value_ptr(projection));
}

void generateSphere(float radius, int stacks, int sectors)
{
    vertices.clear();
    indices.clear();

    for (int i = 0; i <= stacks; ++i)
    {
        const float theta = PI * static_cast<float>(i) / stacks;

        for (int j = 0; j <= sectors; ++j)
        {
            const float phi = 2.0f * PI * static_cast<float>(j) / sectors;

            const float x = radius * sin(theta) * cos(phi);
            const float y = radius * cos(theta);
            const float z = radius * sin(theta) * sin(phi);

            // Position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Normal
            vertices.push_back(x / radius);
            vertices.push_back(y / radius);
            vertices.push_back(z / radius);

            // Texture coordinates
            vertices.push_back(1.0f - static_cast<float>(j) / sectors);
            vertices.push_back(1.0f - static_cast<float>(i) / stacks);
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < sectors; ++j)
        {
            const unsigned int first = i * (sectors + 1) + j;
            const unsigned int second = first + sectors + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
}

// camera position in world space. Earth is a unit sphere at the origin
// the camera always looks along -Z and is moved by pure translation
// arrow keys move it along X and Y, [ and ] move it along Z
glm::vec3 cameraPos(0.0f, 0.0f, 4.0f);

const float MIN_CAMERA_RADIUS = 1.015f; // just above the surface
const float MAX_CAMERA_RADIUS = 50.0f;

// processInput: the only "animation" in model 0.
//   - Arrow keys / [ ] move the camera (translation only).
//     Speed = 1.5 * altitude * deltaTime, so you move slower near Earth.
//     glm::max(altitude, 0.02) makes sure speed never becomes 0.
//   - Camera is kept between MIN and MAX radius by rescaling the vector.
//   - A / D change earthRotation (degrees). main() turns it into
//     glm::rotate(identity, radians(earthRotation), Y axis) = Earth spins.
//   Multiplying by deltaTime = same speed per second on any computer.
void processInput(GLFWwindow* window, float deltaTime, float& earthRotation)
{
    const float rotationSpeed = 70.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // move slower near the surface so close-up views are easy to control
    float altitude = glm::length(cameraPos) - 1.0f;
    float moveSpeed = 1.5f * glm::max(altitude, 0.02f) * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        cameraPos.x -= moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        cameraPos.x += moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cameraPos.y += moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cameraPos.y -= moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        cameraPos.z -= moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        cameraPos.z += moveSpeed;

    // keep the camera outside Earth and within a useful range
    float radius = glm::length(cameraPos);
    if (radius < MIN_CAMERA_RADIUS)
        cameraPos *= MIN_CAMERA_RADIUS / radius;
    else if (radius > MAX_CAMERA_RADIUS)
        cameraPos *= MAX_CAMERA_RADIUS / radius;

    // rotate Earth about its own (Y) axis. glfwGetKey works on physical
    // keys, so both 'a'/'A' and 'd'/'D' are handled.
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        earthRotation -= rotationSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        earthRotation += rotationSpeed;
}

int main()
{
    // Keep this hint if your installed GLFW supports GLFW 3.4.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

    if (!glfwInit())
    {
        cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        1000, 800, "a2-model-0 - Earth Viewer", nullptr, nullptr);

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

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint earthTexture = 0;
    GLuint earthProgram = 0;
    GLuint atmosphereProgram = 0;
    GLuint starProgram = 0;

    try
    {
        generateSphere(1.0f, 64, 64);
        auto earthMesh = make_unique<Mesh>(vertices, indices);

        earthTexture = loadEarthTexture("textures/earth.jpg");
        earthProgram = createShaderProgram(
            "shaders/vertex.glsl", "shaders/fragment.glsl");
        atmosphereProgram = createShaderProgram(
            "shaders/atmosphere_vertex.glsl",
            "shaders/atmosphere_fragment.glsl");
        starProgram = createShaderProgram(
            "shaders/star_vertex.glsl", "shaders/star_fragment.glsl");
        auto stars = make_unique<Starfield>();
        glEnable(GL_PROGRAM_POINT_SIZE);

        cout << "a2-model-0 controls:\n"
             << "  A / D        : rotate Earth about its axis\n"
             << "  Left / Right : move camera along X\n"
             << "  Up / Down    : move camera along Y\n"
             << "  [ / ]        : move camera along Z (towards / away)\n"
             << "  Esc          : exit\n";

        glUseProgram(earthProgram);
        glUniform3f(glGetUniformLocation(earthProgram, "lightPos"),
                    3.0f, 2.0f, 3.0f);
        glUniform1i(glGetUniformLocation(earthProgram, "earthTexture"), 0);

        float earthRotation = 0.0f;
        float lastFrame = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(window))
        {
            const float currentFrame = static_cast<float>(glfwGetTime());
            const float deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            processInput(window, deltaTime, earthRotation);

            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            if (height == 0) height = 1;
            glViewport(0, 0, width, height);

            glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Fixed viewing direction (-Z); only the position changes.
            const glm::mat4 view = glm::lookAt(
                cameraPos,
                cameraPos + glm::vec3(0.0f, 0.0f, -1.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));

            const glm::mat4 projection = glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(width) / static_cast<float>(height),
                0.002f, 200.0f);

            const glm::mat4 earthModel = glm::rotate(
                glm::mat4(1.0f),
                glm::radians(earthRotation),
                glm::vec3(0.0f, 1.0f, 0.0f));

            // Stars in the background (no depth test, drawn first).
            glDisable(GL_DEPTH_TEST);
            setMatrixUniforms(starProgram, glm::mat4(1.0f), view, projection);
            stars->draw();
            glEnable(GL_DEPTH_TEST);

            // Atmosphere first.
            glm::mat4 atmosphereModel = glm::scale(
                earthModel, glm::vec3(1.04f));
            setMatrixUniforms(atmosphereProgram, atmosphereModel, view, projection);
            glUniform3fv(glGetUniformLocation(atmosphereProgram, "cameraPos"),
                         1, glm::value_ptr(cameraPos));

            glDepthMask(GL_FALSE);
            earthMesh->draw();
            glDepthMask(GL_TRUE);

            // Textured Earth.
            setMatrixUniforms(earthProgram, earthModel, view, projection);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, earthTexture);
            earthMesh->draw();
            glBindTexture(GL_TEXTURE_2D, 0);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        // Ensure Mesh is destroyed while the OpenGL context still exists.
        earthMesh.reset();
        stars.reset();
    }
    catch (const exception& e)
    {
        cerr << "Error: " << e.what() << '\n';
    }

    if (earthTexture) glDeleteTextures(1, &earthTexture);
    if (earthProgram) glDeleteProgram(earthProgram);
    if (atmosphereProgram) glDeleteProgram(atmosphereProgram);
    if (starProgram) glDeleteProgram(starProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
