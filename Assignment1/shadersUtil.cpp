#include<GL/glew.h>
#include<iostream>
#include<fstream>
#include<sstream>
#include "shadersUtil.h"
using namespace std;

string readFile(const string& filePath){
    ifstream file(filePath);
    if(!file.is_open()){
        cerr<<"Could not open file\n";
        return "";
    }

    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void vertexShaderCompileLog(unsigned int vertexShader){
    int success;
    char infoLog[512]; 
    glGetShaderiv(vertexShader,GL_COMPILE_STATUS,&success);
    if(!success){
        glGetShaderInfoLog(vertexShader,512,NULL,infoLog);
        cerr<<"Vertex shader compiler faild\n"<<infoLog<<endl;
    }
}

void fragmentShaderCompileLog(unsigned int fragmentShader){
    int success;
    char infoLog[512];
    glGetShaderiv(fragmentShader,GL_COMPILE_STATUS,&success);
    if(!success){
        glGetShaderInfoLog(fragmentShader,512,NULL,infoLog);
        cerr<<"Fragment shader compile failed\n"<<infoLog<<endl;
    }
}

void ShaderLinkingCheck(unsigned int shaderProgram){
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if(!success){
        glGetProgramInfoLog(shaderProgram,512,NULL,infoLog);
        cerr<<"shaderProgram compiler faild\n"<<infoLog<<endl;
    }
}