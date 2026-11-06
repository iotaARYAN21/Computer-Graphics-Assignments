#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 lightPos = vec3(4.0, 8.0, 5.0);

// Ground texture (textures/ground.jpg). If it could not be loaded the
// program leaves useTexture = false and a plain colour is used instead.
uniform sampler2D groundTexture;
uniform bool useTexture = false;
uniform float uvRepeat = 4.0;

void main()
{
    vec3 baseColor = vec3(0.16, 0.42, 0.18);
    if (useTexture)
        baseColor = texture(groundTexture, TexCoord * uvRepeat).rgb;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    float diffuse = max(dot(norm, lightDir), 0.0);
    vec3 ambient = 0.35 * baseColor;
    vec3 litColor = ambient + diffuse * baseColor;

    // Subtle grid-like variation for the untextured fallback.
    if (!useTexture)
    {
        float grid = mod(floor(FragPos.x) + floor(FragPos.z), 2.0);
        litColor *= mix(0.94, 1.0, grid);
    }

    FragColor = vec4(litColor, 1.0);
}
