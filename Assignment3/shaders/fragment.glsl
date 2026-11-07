#version 330 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;
uniform sampler2D earthTexture;
uniform vec3 lightPos;

void main()
{
        // Earth texture color
    vec3 color = texture(earthTexture, TexCoord).rgb;

    // Direction toward the light
    vec3 lightDir = normalize(lightPos - FragPos);

    // Diffuse lighting
    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, lightDir), 0.0);

    // Small ambient component keeps the dark side visible
    float ambient = 0.15;

    vec3 lighting = (ambient + 0.85 * diff) * color;

    FragColor = vec4(lighting, 1.0);


}