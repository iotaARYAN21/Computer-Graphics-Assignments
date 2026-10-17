#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>
#include <vector>

#include <fstream>
#include <sstream>
#include <string>

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

    // Each vertex has three floats: x, y, z.
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        (void*)0
    );

    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    glm::vec3 cameraPos(0.0f, 0.0f, 4.0f);
    // glm::mat4 view = glm::lookAt(cameraPos,glm::vec3(0.0f, 0.0f, 0.0f),glm::vec3(0.0f, 1.0f, 0.0f));
    // glm::mat4 projection = glm::perspective(glm::radians(45.0f),800.0f / 600.0f,0.1f,1000.0f);

    glEnable(GL_DEPTH_TEST);

    GLuint shaderProgram = createShaderProgram();

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.02f, 0.02f, 0.08f, 1.0f);

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        glUseProgram(shaderProgram);

        glm::mat4 model(1.0f);

        glm::mat4 view = glm::lookAt(
            cameraPos,
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            800.0f / 600.0f,
            0.1f,
            1000.0f
        );

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
    glDeleteProgram(shaderProgram);

    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;

}