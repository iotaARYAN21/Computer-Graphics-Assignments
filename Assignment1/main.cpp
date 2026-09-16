#include<GL/glew.h>
#include<GLFW/glfw3.h>
#include<bits/stdc++.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
using namespace std;
// 1x1x1 cube
int cubeX = 2;
int cubeY =2;
int cubeZ = 2;

//color
float colR=1.0f;
float colG=0.0f;
float colB=0.0f;

void key_callback(GLFWwindow* window,int key,int scancode,int action,int mods)
{
    if (action != GLFW_PRESS)
        return;

    if (key==GLFW_KEY_C){
        cout<<"Enter floating point RGB vals (0-1): \n";
        cin>>colR>>colG>>colB;
    }
    if (key == GLFW_KEY_LEFT && cubeX > 0)
        cubeX--;

    if (key == GLFW_KEY_RIGHT && cubeX < 4)
        cubeX++;

    if (key == GLFW_KEY_UP && cubeY < 4)
        cubeY++;

    if (key == GLFW_KEY_DOWN && cubeY > 0)
        cubeY--;

    if (key == GLFW_KEY_U && cubeZ < 4)
        cubeZ++;

    if (key == GLFW_KEY_B && cubeZ > 0)
        cubeZ--;
}

string readFile(const string& filename)
{
    ifstream file(filename);

    if (!file.is_open())
    {
        cerr << "Could not open file: " << filename << endl;
        return "";
    }

    stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}
void generateGrid(vector<float>& a,int n){
    for(int y=0;y<=n;y++){
        for(int z=0;z<=n;z++){
            a.push_back(0);
            a.push_back(y);
            a.push_back(z);

            a.push_back(n);
            a.push_back(y);
            a.push_back(z);
        }
    }

    //y 
    for(int x=0;x<=n;x++){
        for(int z=0;z<=n;z++){
            a.push_back(x);
            a.push_back(0);
            a.push_back(z);

            a.push_back(x);
            a.push_back(n);
            a.push_back(z);
        }
    }

    //z
    for(int x=0;x<=n;x++){
        for(int y=0;y<=n;y++){
            a.push_back(x);
            a.push_back(y);
            a.push_back(0);

            a.push_back(x);
            a.push_back(y);
            a.push_back(n);
        }
    }
}
void generateCube(vector<float>& a, int x, int y, int z)
{
    // 8 corners of the cube
    a = {
        (float)x,     (float)y,     (float)z,       // 0
        (float)x + 1, (float)y,     (float)z,       // 1
        (float)x + 1, (float)y + 1, (float)z,       // 2
        (float)x,     (float)y + 1, (float)z,       // 3

        (float)x,     (float)y,     (float)z + 1,   // 4
        (float)x + 1, (float)y,     (float)z + 1,   // 5
        (float)x + 1, (float)y + 1, (float)z + 1,   // 6
        (float)x,     (float)y + 1, (float)z + 1    // 7
    };
}
int main(){
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

   if(!glfwInit()){
       return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(800,800,"5x5 Grid with 1x1 cube",NULL,NULL);

    if(window==NULL){
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    glewExperimental = GL_TRUE;
    if(glewInit()!=GLEW_OK){
        glfwTerminate();
        return -1;
    }   

    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0f);
    glViewport(0,0,800,800);

    int n=5;
    vector<float> vertices;
    generateGrid(vertices,n);

    vector<float> cubeVertices;

    generateCube(cubeVertices, cubeX, cubeY,cubeZ);
    vector<unsigned int> cubeIndices ={
        // Front face
        4, 5, 6,
        4, 6, 7,

        // Back face
        0, 2, 1,
        0, 3, 2,

        // Left face
        0, 4, 7,
        0, 7, 3,

        // Right face
        1, 2, 6,
        1, 6, 5,

        // Top face
        3, 7, 6,
        3, 6, 2,

        // Bottom face
        0, 1, 5,
        0, 5, 4
    };

    unsigned int cubeVAO;
    unsigned int cubeVBO;
    unsigned int cubeEBO;

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);

// VBO
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);

    glBufferData(GL_ARRAY_BUFFER,cubeVertices.size()*sizeof(float),cubeVertices.data(),GL_STATIC_DRAW);

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,cubeIndices.size()*sizeof(unsigned int),cubeIndices.data(),GL_STATIC_DRAW);

    // Vertex position
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);


    unsigned int VAO,VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);

    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);

    glEnableVertexAttribArray(0);

    string vertexShaderSource =readFile("vertexShader.glsl");

    string fragmentShaderSource = readFile("fragmentShader.glsl");

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

    const char* vertexSource = vertexShaderSource.c_str();

    // glShaderSource(vertexShader,1,&vertexSource,NULL);

    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    GLint success;

    glGetShaderiv( vertexShader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char infoLog[512];

        glGetShaderInfoLog(
            vertexShader,
            512,
            NULL,
            infoLog
        );

        cerr << "Vertex shader error:\n"
             << infoLog << endl;
    }


    GLuint fragmentShader =glCreateShader(GL_FRAGMENT_SHADER);

    const char* fragmentSource =fragmentShaderSource.c_str();

    glShaderSource(
        fragmentShader,
        1,
        &fragmentSource,
        NULL
    );

    glCompileShader(fragmentShader);


    glGetShaderiv(
        fragmentShader,
        GL_COMPILE_STATUS,
        &success
    );

    if (!success)
    {
        char infoLog[512];

        glGetShaderInfoLog(
            fragmentShader,
            512,
            NULL,
            infoLog
        );

        cerr << "Fragment shader error:\n"
             << infoLog << endl;
    }


    GLuint shaderProgram =
        glCreateProgram();

    glAttachShader(
        shaderProgram,
        vertexShader
    );

    glAttachShader(
        shaderProgram,
        fragmentShader
    );

    glLinkProgram(shaderProgram);


    glGetProgramiv(
        shaderProgram,
        GL_LINK_STATUS,
        &success
    );

    if (!success)
    {
        char infoLog[512];

        glGetProgramInfoLog(
            shaderProgram,
            512,
            NULL,
            infoLog
        );

        cerr << "Shader linking error:\n"
             << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint mvpLocation = glGetUniformLocation(shaderProgram,"MVP");
    GLint colorLocation = glGetUniformLocation(shaderProgram,"objectColor");
    while(!glfwWindowShouldClose(window)){
        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 model=glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(10.0f,10.0f,10.0f),glm::vec3(2.5f,2.5f,2.5f),glm::vec3(0.0f,1.0f,0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),800.0f/800.0f,0.1f,100.0f);
        glm::mat4 MVP = projection * view * model;
        glUseProgram(shaderProgram);


        glUniformMatrix4fv(mvpLocation,1,GL_FALSE,glm::value_ptr(MVP));
        // GRID
        glUniform3f(colorLocation, 1.0f, 1.0f, 1.0f);
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES,0,vertices.size()/3);
        
        cubeVertices.clear();
        generateCube(cubeVertices,cubeX,cubeY,cubeZ);
        glBindBuffer(GL_ARRAY_BUFFER,cubeVBO);
        glBufferData(GL_ARRAY_BUFFER,cubeVertices.size()*sizeof(float),cubeVertices.data(),GL_DYNAMIC_DRAW);

        // CUBE
        glUniform3f(colorLocation, colR, colG, colB);
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES,cubeIndices.size(),GL_UNSIGNED_INT,0);
        
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();   
        // glDrawArrays(GL_LINE,)
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}