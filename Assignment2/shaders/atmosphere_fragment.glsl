
#version 330 core

in vec3 Normal;
in vec3 WorldPos;

out vec4 FragColor;

uniform vec3 cameraPos;

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(cameraPos - WorldPos);

    float facing = max(dot(N, V), 0.0);

    // Atmospheric glow concentrated at Earth's silhouette
    float rim = 1.0 - facing;
    float glow = pow(rim, 5.0);

    vec3 atmosphereColor = vec3(0.18, 0.48, 1.0);

    float alpha = glow * 0.22;

    if (alpha < 0.005)
        discard;

    FragColor = vec4(atmosphereColor, alpha);
}
