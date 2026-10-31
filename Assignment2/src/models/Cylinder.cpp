#include "Cylinder.h"

#include <cmath>
#include <vector>

Cylinder::Cylinder(int sectors, bool cone)
{
    const float PI = 3.14159265358979323846f;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Side vertices. Each vertex: position(3), normal(3), uv(2).
    // Radius is 0.5 at the bottom; either 0.5 (cylinder) or 0 (cone) at top.
    for (int i = 0; i <= sectors; ++i)
    {
        float angle = 2.0f * PI * static_cast<float>(i) / sectors;
        float c = std::cos(angle);
        float s = std::sin(angle);

        // Bottom ring
        vertices.insert(vertices.end(), {
            0.5f * c, -0.5f, 0.5f * s,
            c, 0.0f, s,
            static_cast<float>(i) / sectors, 0.0f
        });

        // Top ring (same positions for cylinder; apex radius zero for cone)
        float topRadius = cone ? 0.0f : 0.5f;
        vertices.insert(vertices.end(), {
            topRadius * c, 0.5f, topRadius * s,
            c, cone ? 0.5f : 0.0f, s,
            static_cast<float>(i) / sectors, 1.0f
        });
    }

    for (int i = 0; i < sectors; ++i)
    {
        unsigned int b = static_cast<unsigned int>(2 * i);
        unsigned int t = b + 1;
        unsigned int bn = b + 2;
        unsigned int tn = b + 3;

        indices.insert(indices.end(), {b, bn, t, t, bn, tn});
    }

    // Bottom cap (separate vertices so its normal is correct).
    unsigned int bottomCenter = static_cast<unsigned int>(vertices.size() / 8);
    vertices.insert(vertices.end(), {
        0.0f, -0.5f, 0.0f,  0.0f, -1.0f, 0.0f,  0.5f, 0.5f
    });

    unsigned int bottomStart = static_cast<unsigned int>(vertices.size() / 8);
    for (int i = 0; i < sectors; ++i)
    {
        float angle = 2.0f * PI * static_cast<float>(i) / sectors;
        float c = std::cos(angle);
        float s = std::sin(angle);
        vertices.insert(vertices.end(), {
            0.5f * c, -0.5f, 0.5f * s,
            0.0f, -1.0f, 0.0f,
            0.5f + 0.5f * c, 0.5f + 0.5f * s
        });
    }

    for (int i = 0; i < sectors; ++i)
    {
        unsigned int a = bottomStart + static_cast<unsigned int>(i);
        unsigned int b = bottomStart + static_cast<unsigned int>((i + 1) % sectors);
        indices.insert(indices.end(), {bottomCenter, b, a});
    }

    if (!cone)
    {
        unsigned int topCenter = static_cast<unsigned int>(vertices.size() / 8);
        vertices.insert(vertices.end(), {
            0.0f, 0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  0.5f, 0.5f
        });

        unsigned int topStart = static_cast<unsigned int>(vertices.size() / 8);
        for (int i = 0; i < sectors; ++i)
        {
            float angle = 2.0f * PI * static_cast<float>(i) / sectors;
            float c = std::cos(angle);
            float s = std::sin(angle);
            vertices.insert(vertices.end(), {
                0.5f * c, 0.5f, 0.5f * s,
                0.0f, 1.0f, 0.0f,
                0.5f + 0.5f * c, 0.5f + 0.5f * s
            });
        }

        for (int i = 0; i < sectors; ++i)
        {
            unsigned int a = topStart + static_cast<unsigned int>(i);
            unsigned int b = topStart + static_cast<unsigned int>((i + 1) % sectors);
            indices.insert(indices.end(), {topCenter, a, b});
        }
    }

    mesh = new Mesh(vertices, indices);
}

void Cylinder::draw()
{
    mesh->draw();
}

Cylinder::~Cylinder()
{
    delete mesh;
}
