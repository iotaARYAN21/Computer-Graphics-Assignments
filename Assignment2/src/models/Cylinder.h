#ifndef CYLINDER_H
#define CYLINDER_H

#include "../common/Mesh.h"

class Cylinder
{
private:
    Mesh* mesh;

public:
    // Cylinder is centred at origin, extends from y=-0.5 to y=+0.5.
    // radius is 0.5. Cone mode tapers the top radius to zero.
    explicit Cylinder(int sectors = 48, bool cone = false);
    void draw();
    ~Cylinder();

    Cylinder(const Cylinder&) = delete;
    Cylinder& operator=(const Cylinder&) = delete;
};

#endif
