#ifndef BEZIER_H
#define BEZIER_H

#include <glm/glm.hpp>

#include <string>
#include <vector>

// One trajectory = the control points of a single Bezier curve.
// With n control points the curve has degree n - 1.
using ControlPoints = std::vector<glm::vec3>;

// Point on the curve at parameter u in [0, 1] (de Casteljau's algorithm).
glm::vec3 evaluateBezier(const ControlPoints& points, float u);

// Evenly spaced samples in u, for drawing the curve as a line strip.
std::vector<glm::vec3> sampleBezier(const ControlPoints& points, int samples);

// trajectory.txt format (one block per trajectory):
//   number_of_control_points
//   x_1 y_1 z_1
//   ...
//   x_n y_n z_n
// Further trajectories are appended as more blocks of the same form.
bool appendTrajectory(const std::string& path, const ControlPoints& points);
std::vector<ControlPoints> loadTrajectories(const std::string& path);

#endif
