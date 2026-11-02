#include "Fairing.h"

#include <cmath>
#include <stdexcept>
#include <vector>

Fairing::Fairing(int halfIndex, int sectors)
{
    if (halfIndex != 0 && halfIndex != 1)
        throw std::invalid_argument("halfIndex must be 0 or 1");

    if (sectors < 4)
        throw std::invalid_argument("sectors must be at least 4");

    const float PI = 3.14159265358979323846f;

    const float radius = 0.5f;
    const float height = 1.0f;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Each vertex contains:
    // Position (x,y,z), Normal (nx,ny,nz), UV (u,v).

    // Each half covers 180 degrees.
    const int halfSectors = sectors / 2;
    const float angleStep = PI / static_cast<float>(halfSectors);

    // Rotate the second half by 180 degrees.
    const float startAngle = halfIndex * PI;

    // Generate two vertices per angular segment:
    // one at the bottom and one at the top.
    for (int i = 0; i <= halfSectors; ++i)
    {
        float angle = startAngle + angleStep * i;

        float x = std::cos(angle);
        float z = std::sin(angle);

        // Bottom vertex.
        vertices.insert(vertices.end(), {
            radius * x, -height / 2.0f, radius * z,
            x, 0.0f, z,
            static_cast<float>(i) / halfSectors, 0.0f
        });

        // Top vertex: the radius gradually decreases to zero.
        float topY = height / 2.0f;
        float topRadius = 0.0f;

        vertices.insert(vertices.end(), {
            topRadius * x, topY, topRadius * z,
            x, 0.5f, z,
            static_cast<float>(i) / halfSectors, 1.0f
        });
    }

    // Join neighboring bottom and top vertices.
    for (int i = 0; i < halfSectors; ++i)
    {
        unsigned int bottom =
            static_cast<unsigned int>(2 * i);

        unsigned int top = bottom + 1;

        unsigned int nextBottom = bottom + 2;
        unsigned int nextTop = bottom + 3;

        indices.insert(indices.end(), {
            bottom, nextBottom, top,
            top, nextBottom, nextTop
        });
    }

    mesh = new Mesh(vertices, indices);
}

void Fairing::draw()
{
    mesh->draw();
}

Fairing::~Fairing()
{
    delete mesh;
}