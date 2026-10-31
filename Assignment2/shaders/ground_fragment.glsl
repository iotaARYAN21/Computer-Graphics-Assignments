#version 330 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos = vec3(4.0, 8.0, 5.0);

void main()
{
    vec3 baseColor = vec3(0.16, 0.42, 0.18);
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    float diffuse = max(dot(norm, lightDir), 0.0);
    vec3 ambient = 0.35 * baseColor;
    vec3 litColor = ambient + diffuse * baseColor;

    // Subtle grid-like variation makes the ground easier to read in 3D.
    float grid = mod(floor(FragPos.x) + floor(FragPos.z), 2.0);
    litColor *= mix(0.94, 1.0, grid);

    FragColor = vec4(litColor, 1.0);
}
