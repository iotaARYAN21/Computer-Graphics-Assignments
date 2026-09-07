#include<GL/glew.h>
#include<GLFW/glfw3.h>
#include<bits/stdc++.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
using namespace std;
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
int main(){
    if(glfwInit()==-1){
        return -1;
    }
    GLFWwindow* window = glfwCreateWindow(800,800,"5x5 Grid with 1x1 cube",NULL,NULL);

    glfwMakeContextCurrent(window);
    
    if(window==NULL){
        glfwTerminate();
        return -1;
    }

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

    unsigned int VAO,VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);

    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);


    glEnableVertexAttribArray(0);

        string vertexShaderSource =
        readFile("vertexShader.glsl");

    string fragmentShaderSource =
        readFile("fragmentShader.glsl");

        GLuint vertexShader =
        glCreateShader(GL_VERTEX_SHADER);

    const char* vertexSource =
        vertexShaderSource.c_str();

    glShaderSource(
        vertexShader,
        1,
        &vertexSource,
        NULL
    );

    glCompileShader(vertexShader);

    GLint success;

    glGetShaderiv(
        vertexShader,
        GL_COMPILE_STATUS,
        &success
    );

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


    GLuint fragmentShader =
        glCreateShader(GL_FRAGMENT_SHADER);

    const char* fragmentSource =
        fragmentShaderSource.c_str();

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

    while(!glfwWindowShouldClose(window)){
        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 model=glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(10.0f,10.0f,10.0f),glm::vec3(2.5f,2.5f,2.5f),glm::vec3(0.0f,1.0f,0.0f));
        glm::mat4 projection =
        glm::perspective(
            glm::radians(45.0f),
            800.0f / 800.0f,
            0.1f,
            100.0f
        );
        glm::mat4 MVP =
            projection * view * model;
         glUseProgram(shaderProgram);


        GLint mvpLocation =
            glGetUniformLocation(
                shaderProgram,
                "MVP"
            );

        glUniformMatrix4fv(
            mvpLocation,
            1,
            GL_FALSE,
            glm::value_ptr(MVP)
        );

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES,0,vertices.size()/3);
        glBindVertexArray(0);
        glfwSwapBuffers(window);
        glfwPollEvents();   
        // glDrawArrays(GL_LINE,)
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}