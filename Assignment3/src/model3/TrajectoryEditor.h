#ifndef TRAJECTORY_EDITOR_H
#define TRAJECTORY_EDITOR_H

#include "Bezier.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

// Interactive Bezier trajectory editor.
// The editor has its own camera that orbits a "focus point". A left click
// places a control point where the mouse ray meets the plane through the
// focus point facing the camera, or on Earth's surface if the surface is
// hit first (points can never end up inside Earth).
class TrajectoryEditor
{
public:
    bool active = false;

    // Editor camera (world units).
    glm::vec3 focus{0.0f};
    float yaw = -150.0f; // same sunlit side as the launch-site camera
    float pitch = 15.0f;
    float distance = 1.0f;

    ControlPoints current;              // trajectory being edited
    std::vector<ControlPoints> loaded;  // trajectories read from the file
    bool showLoaded = false;

    std::string filePath = "trajectory.txt";
    std::string status;                 // short message for the title bar

    void localFrame(glm::vec3& east, glm::vec3& up, glm::vec3& north) const;
    glm::vec3 cameraPosition() const;
    glm::mat4 viewMatrix() const;
    float nearPlane() const;

    // Continuous controls (arrow keys etc.), called every frame.
    void updateMovement(GLFWwindow* window, float deltaTime);

    // One-shot actions.
    void saveCurrent();      // T: append current trajectory to the file
    void loadAll();          // Y: load every trajectory and show them
    void newTrajectory();    // N: start an empty trajectory
    void removeLastPoint();  // Backspace / right click
    void focusOnLastPoint(); // F

    // Mouse placement. Cursor position in window coordinates.
    void placePoint(double cursorX, double cursorY, int windowWidth,
                    int windowHeight, const glm::mat4& projection,
                    float earthRadius);
};

// Distinct colours for the loaded curves (cycled).
glm::vec3 trajectoryColour(int index);

#endif
