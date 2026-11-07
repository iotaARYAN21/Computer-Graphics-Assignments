#include "Bezier.h"
#include <fstream>
#include <iomanip>
#include <iostream>
//Calculate a point on a Bezier curve
glm::vec3 evaluateBezier(const ControlPoints& points,float u)
{
    if(points.empty())
        return glm::vec3(0.0f);

    // Repeated linear Interpolation between neighbouring points.
    ControlPoints work = points;
    for (size_t level = work.size() - 1; level > 0; --level)
        for (size_t i = 0; i < level; ++i)
            work[i] = glm::mix(work[i], work[i + 1], u);
    return work[0];
}

std::vector<glm::vec3> sampleBezier(const ControlPoints& points, int samples)
{
    std::vector<glm::vec3> result;
    if (points.size() < 2 || samples < 2)
        return result;

    result.reserve(samples);
    for (int i = 0; i < samples; ++i)
        result.push_back(evaluateBezier(points, static_cast<float>(i) / (samples - 1)));
    return result;
}
//Save control points
bool appendTrajectory(const std::string& path, const ControlPoints& points)
{
    std::ofstream file(path, std::ios::app);
    if (!file)
        return false;

    file << points.size() << '\n';
    file << std::fixed << std::setprecision(6);
    for (const glm::vec3& p : points)
        file << p.x << ' ' << p.y << ' ' << p.z << '\n';
    return static_cast<bool>(file);
}

std::vector<ControlPoints> loadTrajectories(const std::string& path)
{
    std::vector<ControlPoints> result;
    std::ifstream file(path);
    if (!file)
        return result;

    int count = 0;
    while (file >> count)
    {   if (count <= 0)
        {
            std::cerr << "trajectory file: bad point count " << count << '\n';
            break;
        }
        ControlPoints points;
        for (int i = 0; i < count; ++i)
        {
            glm::vec3 p;
            if (!(file >> p.x >> p.y >> p.z))
            {
                std::cerr << "trajectory file: block " << result.size() + 1
                          << " ends early\n";
                return result;
            }
            points.push_back(p);
        }
        result.push_back(points);
    }
    return result;
}
