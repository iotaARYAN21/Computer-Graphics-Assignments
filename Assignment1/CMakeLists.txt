cmake_minimum_required(VERSION 3.10)

project(assignment1)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(main main.cpp)
target_include_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/../../glm
)
target_link_libraries(main OpenGL::GL GLEW::GLEW glfw )