#ifndef MESH_H
#define MESH_H

#include <GL/glew.h>
#include <vector>

class Mesh
{
private:
    GLuint VAO;
    GLuint VBO;
    GLuint EBO;

    GLsizei indexCount;

public:
    Mesh(
        const std::vector<float>& vertices,
        const std::vector<unsigned int>& indices
    );

    void draw();

    ~Mesh();

    // Prevent copying OpenGL resource ownership.
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
};

#endif