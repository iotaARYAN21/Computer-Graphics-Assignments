#ifndef STARFIELD_H
#define STARFIELD_H

#include <GL/glew.h>

// Random stars on a large sphere around the camera, drawn as GL_POINTS.
// Vertex layout: position (3 floats), brightness (1 float).
// The star shader removes the view translation, so the stars behave like
// a background at infinity (an environment sphere).
class Starfield
{
private:
    GLuint VAO;
    GLuint VBO;
    GLsizei count;

public:
    explicit Starfield(int starCount = 3000, float radius = 100.0f,
                       unsigned int seed = 5024);
    void draw();
    ~Starfield();

    Starfield(const Starfield&) = delete;
    Starfield& operator=(const Starfield&) = delete;
};

#endif
