#include "Cylinder.h"

#include <cmath>
#include <vector>

Cylinder::Cylinder(int sectors, bool cone, bool halfCone)
{
    const float PI = 3.14159265358979323846f;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // A full shape uses all sectors.
    // A half-cone uses half of the angular range.
    int actualSectors = halfCone ? sectors / 2 : sectors;

    if (actualSectors < 3)
        actualSectors = 3;

    // Use the original sector count for angular spacing.
    // This ensures halfCone covers exactly half a revolution.
    float angleStep = 2.0f * PI / static_cast<float>(sectors);


    // SIDE VERTICES
    // Each vertex: position(3), normal(3), UV(2)

    for (int i = 0; i <= actualSectors; ++i)
    {
        float angle = angleStep * static_cast<float>(i);

        float c = std::cos(angle);
        float s = std::sin(angle);

        // Bottom ring.
        vertices.insert(vertices.end(), {
            0.5f * c, -0.5f, 0.5f * s,
            c, 0.0f, s,
            static_cast<float>(i) / static_cast<float>(actualSectors), 0.0f
        });

        // Top ring.
        float topRadius = cone ? 0.0f : 0.5f;

        vertices.insert(vertices.end(), {
            topRadius * c, 0.5f, topRadius * s,
            c, cone ? 0.5f : 0.0f, s,
            static_cast<float>(i) / static_cast<float>(actualSectors), 1.0f
        });
    }
    
    // SIDE TRIANGLES
    

    for (int i = 0; i < actualSectors; ++i)
    {
        unsigned int b = static_cast<unsigned int>(2 * i);
        unsigned int t = b + 1;

        unsigned int bn = b + 2;
        unsigned int tn = b + 3;

        indices.insert(indices.end(), {
            b, bn, t,
            t, bn, tn
        });
    }

    
    // BOTTOM CAP
    

    unsigned int bottomCenter =
        static_cast<unsigned int>(vertices.size() / 8);

    vertices.insert(vertices.end(), {
        0.0f, -0.5f, 0.0f,
        0.0f, -1.0f, 0.0f,
        0.5f, 0.5f
    });

    unsigned int bottomStart =
        static_cast<unsigned int>(vertices.size() / 8);

    for (int i = 0; i < actualSectors; ++i)
    {
        float angle = angleStep * static_cast<float>(i);

        float c = std::cos(angle);
        float s = std::sin(angle);

        vertices.insert(vertices.end(), {
            0.5f * c, -0.5f, 0.5f * s,
            0.0f, -1.0f, 0.0f,
            0.5f + 0.5f * c,
            0.5f + 0.5f * s
        });
    }

    for (int i = 0; i < actualSectors; ++i)
    {
        unsigned int a =
            bottomStart + static_cast<unsigned int>(i);

        unsigned int b =
            bottomStart +
            static_cast<unsigned int>((i + 1) % actualSectors);

        indices.insert(indices.end(), {
            bottomCenter, b, a
        });
    }

    
    // TOP CAP
    // Cylinders need a top cap; cones do not.
    

    if (!cone)
    {
        unsigned int topCenter =
            static_cast<unsigned int>(vertices.size() / 8);

        vertices.insert(vertices.end(), {
            0.0f, 0.5f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.5f, 0.5f
        });

        unsigned int topStart =
            static_cast<unsigned int>(vertices.size() / 8);

        for (int i = 0; i < actualSectors; ++i)
        {
            float angle = angleStep * static_cast<float>(i);

            float c = std::cos(angle);
            float s = std::sin(angle);

            vertices.insert(vertices.end(), {
                0.5f * c, 0.5f, 0.5f * s,
                0.0f, 1.0f, 0.0f,
                0.5f + 0.5f * c,
                0.5f + 0.5f * s
            });
        }

        for (int i = 0; i < actualSectors; ++i)
        {
            unsigned int a =
                topStart + static_cast<unsigned int>(i);

            unsigned int b =
                topStart +
                static_cast<unsigned int>((i + 1) % actualSectors);

            indices.insert(indices.end(), {
                topCenter, a, b
            });
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