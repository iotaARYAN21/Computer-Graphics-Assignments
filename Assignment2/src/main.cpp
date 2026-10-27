#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cmath>
#include <iostream>
#include <vector>

#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>

using namespace std;

const float PI = 3.14159265359f;

vector<float> vertices;
vector<unsigned int> indices;

string readFile(const string& path)
{
    ifstream file(path);

    if (!file.is_open())
    {
        throw runtime_error("Cannot open shader: " + path);
    }

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

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);

        cerr << "Shader compilation error:\n"
             << log << endl;

        glDeleteShader(shader);
        throw runtime_error("Shader compilation failed");
    }

    return shader;
}

GLuint createShaderProgram()
{
    string vertexSource = readFile("shaders/vertex.glsl");
    string fragmentSource = readFile("shaders/fragment.glsl");

    GLuint vertexShader = compileShader(
        GL_VERTEX_SHADER, vertexSource
    );

    GLuint fragmentShader = compileShader(
        GL_FRAGMENT_SHADER, fragmentSource
    );

    GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetProgramInfoLog(program, 1024, nullptr, log);

        cerr << "Shader linking error:\n"
             << log << endl;

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);

        throw runtime_error("Shader linking failed");
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

GLuint loadEarthTexture(const char* path)
{
    int width, height, channels;

    // Keep image orientation consistent with the UV coordinates.
    stbi_set_flip_vertically_on_load(true);

    unsigned char* image = stbi_load(
        path, &width, &height, &channels, 0);

    if (!image)
        throw runtime_error(
            string("Cannot load Earth texture: ") + path +
            "\n" + stbi_failure_reason());

    cout << "Earth texture: " << width << " x " << height
         << ", channels: " << channels << endl;

    GLenum format = GL_RGB;

    if (channels == 1)
        format = GL_RED;
    else if (channels == 3)
        format = GL_RGB;
    else if (channels == 4)
        format = GL_RGBA;
    else
    {
        stbi_image_free(image);
        throw runtime_error("Unsupported Earth texture format");
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Avoid incorrect row alignment for RGB images.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format,
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        image
    );

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR);

    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(image);

    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    return texture;
}
void processInput(GLFWwindow* window,glm::vec3& cameraPos,float deltaTime, float& earthRotation)
{
    float speed = 2.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    // Move camera along X.
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        cameraPos.x -= speed;

    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        cameraPos.x += speed;

    // Move camera along Y.
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cameraPos.y += speed;

    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cameraPos.y -= speed;

    // Move camera along Z.
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        cameraPos.z -= speed;

    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        cameraPos.z += speed;

    //Earth Rotation.
    float manualSpeed = 90.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        earthRotation -= manualSpeed;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        earthRotation += manualSpeed;

}

void generateSphere(float radius, int stacks, int sectors)
{
    vertices.clear();
    indices.clear();

    // Generate the sphere's vertices.
    for (int i = 0; i <= stacks; i++)
    {
        float theta = PI * i / stacks;

        for (int j = 0; j <= sectors; j++)
        {
            float phi = 2.0f * PI * j / sectors;

            float x = radius * sin(theta) * cos(phi);
            float y = radius * cos(theta);
            float z = radius * sin(theta) * sin(phi);

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Surface normal (unit vector)
            vertices.push_back(x / radius);
            vertices.push_back(y / radius);
            vertices.push_back(z / radius);

            // Texture coordinates: U = longitude, V = latitude.
            float u = 1.0f - static_cast<float>(j) / sectors;
            float v = 1.0f - static_cast<float>(i) / stacks;

            
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    // Connect neighbouring vertices to form triangles.
    for (int i = 0; i < stacks; i++)
    {
        for (int j = 0; j < sectors; j++)
        {
            unsigned int first =
                i * (sectors + 1) + j;

            unsigned int second =
                first + sectors + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
}

int main(){
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

    if(!glfwInit()){
       return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(800,800,"Assignment-2",NULL,NULL);

    if(window==NULL){
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if(glewInit()!=GLEW_OK){
        glfwTerminate();
        return -1;
    }   

    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0f);
    glViewport(0,0,800,800);

    GLuint VAO, VBO, EBO;

    generateSphere(1.0f, 40, 40);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Upload vertex positions.
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float),
        vertices.data(),
        GL_STATIC_DRAW
    );

    // Upload triangle indices.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW
    );

    // Each vertex has 8 floats: x, y, z, 3 normal,u,v.
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        8 * sizeof(float),
        (void*)0
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE,
            8 * sizeof(float),
            (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE,
        8 * sizeof(float), (void*)(6 * sizeof(float))
    );
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // Load Earth's image into an OpenGL texture.
        GLuint earthTexture = loadEarthTexture(
            "textures/earth.jpg");

        GLuint shaderProgram = createShaderProgram();

        glUseProgram(shaderProgram);
        glUniform3f(
        glGetUniformLocation(shaderProgram, "lightPos"), 3.0f, 2.0f, 3.0f);
        glUniform1i(
            glGetUniformLocation(shaderProgram, "earthTexture"), 0);


    glm::vec3 cameraPos(0.0f, 0.0f, 4.0f);
    // glm::mat4 view = glm::lookAt(cameraPos,glm::vec3(0.0f, 0.0f, 0.0f),glm::vec3(0.0f, 1.0f, 0.0f));
    // glm::mat4 projection = glm::perspective(glm::radians(45.0f),800.0f / 600.0f,0.1f,1000.0f);

    glEnable(GL_DEPTH_TEST);

    // rotation part
    float earthRotation = 0.0f;
    float rotationSpeed = 30.0f;  // Degrees per second

    float cameraSpeed = 2.0f;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.02f, 0.02f, 0.08f, 1.0f);

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        glUseProgram(shaderProgram);

        float currentFrame = static_cast<float>(glfwGetTime());

        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window, cameraPos, deltaTime, earthRotation);

        earthRotation += rotationSpeed * deltaTime;
        glm::mat4 model(1.0f);
        model = glm::rotate(model,glm::radians(earthRotation),glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 view = glm::lookAt(
            cameraPos,
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            800.0f / 800.0f,
            0.1f,
            1000.0f
        );
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(
            glGetUniformLocation(shaderProgram, "model"),
            1, GL_FALSE, glm::value_ptr(model)
        );

        glUniformMatrix4fv(
            glGetUniformLocation(shaderProgram, "view"),
            1, GL_FALSE, glm::value_ptr(view)
        );

        glUniformMatrix4fv(
            glGetUniformLocation(shaderProgram, "projection"),
            1, GL_FALSE, glm::value_ptr(projection)
        );
        // Bind the Earth texture to texture unit 0.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, earthTexture);

        glBindVertexArray(VAO);

        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(indices.size()),
            GL_UNSIGNED_INT,
            nullptr
        );

        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup before exiting.
    glDeleteTextures(1, &earthTexture);
    glDeleteProgram(shaderProgram);

    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;

}