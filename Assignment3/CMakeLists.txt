cmake_minimum_required(VERSION 3.10)

project(assignment3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

# Executable 1: Earth viewer
add_executable(a2-model-0
    src/model0/main.cpp
    src/common/Mesh.cpp
    src/common/Starfield.cpp
)

# Executable 2: Launch-site viewer
add_executable(a2-model-1
    src/model1/main.cpp
    src/common/Mesh.cpp
    src/models/Cube.cpp
    src/models/Plane.cpp
)

target_include_directories(a2-model-0 PRIVATE ${CMAKE_SOURCE_DIR}/glm)
target_link_libraries(a2-model-0 PRIVATE OpenGL::GL GLEW::GLEW glfw)

target_include_directories(a2-model-1 PRIVATE
    ${CMAKE_SOURCE_DIR}/glm
)
target_link_libraries(a2-model-1 PRIVATE OpenGL::GL GLEW::GLEW glfw)

#Executable 3: Vehicle and payload viewer
add_executable(a2-model-2
    src/model2/main.cpp
    src/common/Mesh.cpp
    src/models/Cylinder.cpp
    src/models/Cube.cpp
    src/models/Fairing.cpp
)
target_include_directories(a2-model-2 PRIVATE ${CMAKE_SOURCE_DIR}/glm)
target_link_libraries(a2-model-2 PRIVATE OpenGL::GL GLEW::GLEW glfw)


# Executable 4: Integrated scene with trajectory editor and playback
add_executable(a3-scene
    src/model3/main.cpp
    src/model3/Bezier.cpp
    src/model3/Keyframes.cpp
    src/model3/TrajectoryEditor.cpp
    src/common/Mesh.cpp
    src/common/Starfield.cpp
    src/models/Cylinder.cpp
    src/models/Cube.cpp
    src/models/Fairing.cpp
    src/models/Plane.cpp
)
target_include_directories(a3-scene PRIVATE ${CMAKE_SOURCE_DIR}/glm)
target_link_libraries(a3-scene PRIVATE OpenGL::GL GLEW::GLEW glfw)

# Executable 5: trajectory and keyframe planner (writes trajectory.txt and
# keyframes.txt from the flight model)
add_executable(a3-plan
    src/model3/Planner.cpp
    src/model3/Flight.cpp
    src/model3/Bezier.cpp
    src/model3/Keyframes.cpp
)
target_include_directories(a3-plan PRIVATE ${CMAKE_SOURCE_DIR}/glm)
