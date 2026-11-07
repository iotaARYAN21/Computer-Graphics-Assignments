#version 330 core

in float Brightness;

out vec4 FragColor;

// 1.0 in space, lowered when the camera is inside the daytime sky.
uniform float starVisibility = 1.0;

void main()
{
    FragColor = vec4(vec3(0.85, 0.88, 1.0), Brightness * starVisibility);
}
