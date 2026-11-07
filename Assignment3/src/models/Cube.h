#ifndef CUBE_H
#define CUBE_H

#include "../common/Mesh.h"

class Cube
{
private:
    Mesh* mesh;

public:
    Cube();

    void draw();

    ~Cube();

    Cube(const Cube&) = delete;
    Cube& operator=(const Cube&) = delete;
};

#endif