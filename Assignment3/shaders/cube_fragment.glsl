#version 330 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 objectColor;

// Used by the exhaust plume in a2-scene. The defaults give the normal
// lit, opaque look used everywhere else.
uniform float emissive = 0.0; // 1.0 = self-lit, ignores the light
uniform float alpha = 1.0;

void main()
{
    // Fully self-lit (exhaust, Sun, lines): skip the lighting so geometry
    // without normals (line strips) cannot produce NaN colours.
    if (emissive >= 1.0)
    {
        FragColor = vec4(objectColor, alpha);
        return;
    }

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    float diff = max(dot(norm, lightDir), 0.0);

    vec3 ambient = 0.2 * objectColor;
    vec3 diffuse = diff * objectColor;

    vec3 color = mix(ambient + diffuse, objectColor, emissive);
    FragColor = vec4(color, alpha);
}
