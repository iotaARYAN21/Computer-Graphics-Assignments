cmake_minimum_required(VERSION 3.10)

project(assignment2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(a2-model-0 src/main.cpp)

target_include_directories(a2-model-0 PRIVATE ${CMAKE_SOURCE_DIR}/../../glm)
target_link_libraries(a2-model-0 PRIVATE OpenGL::GL GLEW::GLEW glfw)