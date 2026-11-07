#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in float aBrightness;

out float Brightness;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Drop the translation part of the view matrix so the stars stay
    // "infinitely" far away and only react to camera rotation.
    mat4 rotationOnly = mat4(mat3(view));

    Brightness = aBrightness;
    gl_PointSize = 1.0 + 1.5 * aBrightness;
    gl_Position = projection * rotationOnly * vec4(aPos, 1.0);
}
