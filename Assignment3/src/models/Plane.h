#ifndef PLANE_H
#define PLANE_H

#include "../common/Mesh.h"

class Plane
{
private:
    Mesh* mesh;

public:
    Plane();
    void draw();
    ~Plane();

    Plane(const Plane&) = delete;
    Plane& operator=(const Plane&) = delete;
};

#endif