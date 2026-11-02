#ifndef FAIRING_H
#define FAIRING_H

#include "../common/Mesh.h"

class Fairing
{
private:
    Mesh* mesh;

public:
    // Creates one half of the payload fairing.
    // halfIndex = 0 creates the first half.
    // halfIndex = 1 creates the second half.
    explicit Fairing(int halfIndex, int sectors = 36);

    void draw();

    ~Fairing();

    Fairing(const Fairing&) = delete;
    Fairing& operator=(const Fairing&) = delete;
};

#endif