#include "Plane.h"
#include <vector>

using namespace std;

Plane::Plane()
{
    vector<float> vertices = {
    // Position             Normal       Texture coordinates

    -10.0f, 0.0f, -10.0f,  0, 1, 0,     0, 0,
     10.0f, 0.0f, -10.0f,  0, 1, 0,     1, 0,
     10.0f, 0.0f,  10.0f,  0, 1, 0,     1, 1,
    -10.0f, 0.0f,  10.0f,  0, 1, 0,     0, 1
};

    vector<unsigned int> indices = {
        0, 1, 2,
        0, 2, 3
    };

    mesh = new Mesh(vertices, indices);
}

void Plane::draw()
{
    mesh->draw();
}

Plane::~Plane()
{
    delete mesh;
}