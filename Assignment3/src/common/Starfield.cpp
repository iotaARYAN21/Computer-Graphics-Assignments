#include "Starfield.h"

#include <cmath>
#include <random>
#include <vector>

Starfield::Starfield(int starCount, float radius, unsigned int seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);

    std::vector<float> data;
    data.reserve(static_cast<size_t>(starCount) * 4);

    for (int i = 0; i < starCount; ++i)
    {
        // Uniformly distributed direction on the unit sphere.
        float y = 2.0f * uniform(rng) - 1.0f;
        float angle = 2.0f * 3.14159265f * uniform(rng);
        float r = std::sqrt(1.0f - y * y);

        // Most stars are faint, only a few are bright.
        float brightness = 0.25f + 0.75f * std::pow(uniform(rng), 3.0f);

        data.push_back(radius * r * std::cos(angle));
        data.push_back(radius * y);
        data.push_back(radius * r * std::sin(angle));
        data.push_back(brightness);
    }

    count = static_cast<GLsizei>(starCount);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                 data.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = 4 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride,
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Starfield::draw()
{
    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, count);
    glBindVertexArray(0);
}

Starfield::~Starfield()
{
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
}
