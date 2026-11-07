#version 330 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 objectColor;

void main()
{
    vec3 n = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diffuse = max(dot(n, lightDir), 0.0);

    vec3 ambient = 0.28 * objectColor;
    vec3 diffuseColor = 0.85 * diffuse * objectColor;

    FragColor = vec4(ambient + diffuseColor, 1.0);
}
