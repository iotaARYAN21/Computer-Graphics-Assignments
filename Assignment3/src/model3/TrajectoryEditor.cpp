#include "TrajectoryEditor.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdio>
#include <iostream>

glm::vec3 trajectoryColour(int index)
{
    static const glm::vec3 colours[] = {
        {1.00f, 0.35f, 0.25f}, // red
        {0.30f, 0.85f, 0.35f}, // green
        {0.35f, 0.60f, 1.00f}, // blue
        {1.00f, 0.85f, 0.20f}, // yellow
        {0.85f, 0.40f, 1.00f}, // purple
        {0.25f, 0.90f, 0.90f}, // cyan
        {1.00f, 0.60f, 0.15f}, // orange
        {0.95f, 0.95f, 0.95f}, // white
    };
    return colours[index % 8];
}

// The camera orbits in a local frame at the focus point whose "up" is the
// local vertical (away from Earth's centre), so the horizon stays level
// near the launch site as well as far away.
void TrajectoryEditor::localFrame(glm::vec3& east, glm::vec3& up, glm::vec3& north) const
{
    up = glm::length(focus) > 1e-3f ? glm::normalize(focus) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 side = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), up);
    east = glm::length(side) > 1e-3f ? glm::normalize(side) : glm::vec3(1.0f, 0.0f, 0.0f);
    north = glm::cross(up, east);
}

glm::vec3 TrajectoryEditor::cameraPosition() const
{
    glm::vec3 east, up, north;
    localFrame(east, up, north);
    float y = glm::radians(yaw);
    float p = glm::radians(pitch);
    glm::vec3 dir = east * (std::cos(p) * std::sin(y)) + up * std::sin(p) + north * (std::cos(p) * std::cos(y));
    return focus + dir * distance;
}

glm::mat4 TrajectoryEditor::viewMatrix() const
{
    glm::vec3 east, up, north;
    localFrame(east, up, north);
    return glm::lookAt(cameraPosition(), focus, up);
}

float TrajectoryEditor::nearPlane() const
{
    return glm::clamp(distance * 0.002f, 0.0002f, 1.0f);
}

void TrajectoryEditor::updateMovement(GLFWwindow* window, float deltaTime)
{
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                 glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    auto down = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

    if (!shift)
    {
        // Arrow keys orbit the camera around the focus point.
        float angle = 60.0f * deltaTime;
        if (down(GLFW_KEY_LEFT)) yaw -= angle;
        if (down(GLFW_KEY_RIGHT)) yaw += angle;
        if (down(GLFW_KEY_UP)) pitch = glm::min(pitch + angle, 89.0f);
        if (down(GLFW_KEY_DOWN)) pitch = glm::max(pitch - angle, -89.0f);
    }
    else
    {
        // Shift + arrow keys move the focus point across the screen.
        glm::vec3 east, localUp, north;
        localFrame(east, localUp, north);
        glm::vec3 forward = glm::normalize(focus - cameraPosition());
        glm::vec3 right = glm::normalize(glm::cross(forward, localUp));
        glm::vec3 up = glm::cross(right, forward);
        float step = distance * 0.8f * deltaTime;
        if (down(GLFW_KEY_LEFT)) focus -= right * step;
        if (down(GLFW_KEY_RIGHT)) focus += right * step;
        if (down(GLFW_KEY_UP)) focus += up * step;
        if (down(GLFW_KEY_DOWN)) focus -= up * step;
    }

    // Page Up / Page Down move the focus point into / out of the screen.
    glm::vec3 forward = glm::normalize(focus - cameraPosition());
    float step = distance * 0.8f * deltaTime;
    if (down(GLFW_KEY_PAGE_UP)) focus += forward * step;
    if (down(GLFW_KEY_PAGE_DOWN)) focus -= forward * step;

    // [ and ] zoom (multiplicative, so it works from pad scale to Mars).
    float zoom = 1.0f + 1.5f * deltaTime;
    if (down(GLFW_KEY_LEFT_BRACKET)) distance = glm::max(distance / zoom, 0.01f);
    if (down(GLFW_KEY_RIGHT_BRACKET)) distance = glm::min(distance * zoom, 3000.0f);
}

void TrajectoryEditor::saveCurrent()
{
    if (current.size() < 2)
    {
        status = "need at least 2 points to save";
        return;
    }
    if (!appendTrajectory(filePath, current))
    {
        status = "could not write " + filePath;
        return;
    }
    char text[128];
    std::snprintf(text, sizeof(text), "saved %zu points to %s",
                  current.size(), filePath.c_str());
    status = text;
    std::cout << text << '\n';
}

void TrajectoryEditor::loadAll()
{
    loaded = loadTrajectories(filePath);
    showLoaded = true;
    char text[128];
    std::snprintf(text, sizeof(text), "loaded %zu trajectories from %s",
                  loaded.size(), filePath.c_str());
    status = text;
    std::cout << text << '\n';
    for (size_t i = 0; i < loaded.size(); ++i)
        std::cout << "  trajectory " << i + 1 << ": "
                  << loaded[i].size() << " control points\n";
}

void TrajectoryEditor::newTrajectory()
{
    current.clear();
    status = "new trajectory";
}

void TrajectoryEditor::removeLastPoint()
{
    if (!current.empty())
    {
        current.pop_back();
        status = "removed last point";
    }
}

void TrajectoryEditor::focusOnLastPoint()
{
    if (!current.empty())
        focus = current.back();
}

void TrajectoryEditor::placePoint(double cursorX, double cursorY,
                                  int windowWidth, int windowHeight,
                                  const glm::mat4& projection, float earthRadius)
{
    if (windowWidth <= 0 || windowHeight <= 0)
        return;

    // Ray from the camera through the cursor.
    float ndcX = static_cast<float>(2.0 * cursorX / windowWidth - 1.0);
    float ndcY = static_cast<float>(1.0 - 2.0 * cursorY / windowHeight);
    glm::mat4 inverseVP = glm::inverse(projection * viewMatrix());
    glm::vec4 nearPoint = inverseVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w;
    glm::vec3 dir = glm::normalize(glm::vec3(farPoint) / farPoint.w - origin);

    // Plane through the focus point, facing the camera.
    glm::vec3 normal = glm::normalize(cameraPosition() - focus);
    float denom = glm::dot(dir, normal);
    float tPlane = -1.0f;
    if (std::fabs(denom) > 1e-6f)
        tPlane = glm::dot(focus - origin, normal) / denom;

    // Earth's surface (sphere at the origin).
    float b = glm::dot(origin, dir);
    float c = glm::dot(origin, origin) - earthRadius * earthRadius;
    float disc = b * b - c;
    float tEarth = -1.0f;
    if (disc >= 0.0f)
        tEarth = -b - std::sqrt(disc);

    glm::vec3 point;
    if (tEarth > 0.0f && (tPlane <= 0.0f || tEarth < tPlane))
    {
        // Snap onto the surface, raised very slightly.
        point = origin + dir * tEarth;
        point = glm::normalize(point) * (earthRadius * 1.00002f);
    }
    else if (tPlane > 0.0f)
    {
        point = origin + dir * tPlane;
    }
    else
    {
        return;
    }

    current.push_back(point);
    char text[128];
    std::snprintf(text, sizeof(text), "point %zu at (%.4f, %.4f, %.4f)",
                  current.size(), point.x, point.y, point.z);
    status = text;
    std::cout << text << '\n';
}
