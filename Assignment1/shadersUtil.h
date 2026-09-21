
#ifndef SHADERS_UTIL_H
#define SHADERS_UTIL_H
#include<string>
std::string readFile(const std::string& filePath);

void vertexShaderCompileLog(unsigned int vertexShader);

void fragmentShaderCompileLog(unsigned int fragmentShader);

void ShaderLinkingCheck(unsigned int shaderProgram);
#endif