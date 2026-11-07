#include "Cube.h"

#include <vector>
#include <memory>

using namespace std;

Cube::Cube()
{
    vector<float> vertices;
    vector<unsigned int> indices;

    // Each vertex contains:
    // Position (x,y,z), Normal (nx,ny,nz), Texture coordinates (u,v).

    // Front face: z = 0.5
    float front[] = {
        -0.5f, -0.5f,  0.5f,  0, 0, 1,  0, 0,
         0.5f, -0.5f,  0.5f,  0, 0, 1,  1, 0,
         0.5f,  0.5f,  0.5f,  0, 0, 1,  1, 1,
        -0.5f,  0.5f,  0.5f,  0, 0, 1,  0, 1
    };

    // Back face: z = -0.5
    float back[] = {
         0.5f, -0.5f, -0.5f,  0, 0, -1,  0, 0,
        -0.5f, -0.5f, -0.5f,  0, 0, -1,  1, 0,
        -0.5f,  0.5f, -0.5f,  0, 0, -1,  1, 1,
         0.5f,  0.5f, -0.5f,  0, 0, -1,  0, 1
    };

    // Left face: x = -0.5
    float left[] = {
        -0.5f, -0.5f, -0.5f, -1, 0, 0,  0, 0,
        -0.5f, -0.5f,  0.5f, -1, 0, 0,  1, 0,
        -0.5f,  0.5f,  0.5f, -1, 0, 0,  1, 1,
        -0.5f,  0.5f, -0.5f, -1, 0, 0,  0, 1
    };

    // Right face: x = 0.5
    float right[] = {
         0.5f, -0.5f,  0.5f,  1, 0, 0,  0, 0,
         0.5f, -0.5f, -0.5f,  1, 0, 0,  1, 0,
         0.5f,  0.5f, -0.5f,  1, 0, 0,  1, 1,
         0.5f,  0.5f,  0.5f,  1, 0, 0,  0, 1
    };

    // Top face: y = 0.5
    float top[] = {
        -0.5f, 0.5f,  0.5f,  0, 1, 0,  0, 0,
         0.5f, 0.5f,  0.5f,  0, 1, 0,  1, 0,
         0.5f, 0.5f, -0.5f,  0, 1, 0,  1, 1,
        -0.5f, 0.5f, -0.5f,  0, 1, 0,  0, 1
    };

    // Bottom face: y = -0.5
    float bottom[] = {
        -0.5f, -0.5f, -0.5f,  0, -1, 0,  0, 0,
         0.5f, -0.5f, -0.5f,  0, -1, 0,  1, 0,
         0.5f, -0.5f,  0.5f,  0, -1, 0,  1, 1,
        -0.5f, -0.5f,  0.5f,  0, -1, 0,  0, 1
    };

    float* faces[] = {
        front, back, left, right, top, bottom
    };

    for (int face = 0; face < 6; ++face)
    {
        unsigned int base = face * 4;

        for (int i = 0; i < 32; ++i)
            vertices.push_back(faces[face][i]);

        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    mesh = new Mesh(vertices, indices);
}

void Cube::draw()
{
    mesh->draw();
}

Cube::~Cube()
{
    delete mesh;
}