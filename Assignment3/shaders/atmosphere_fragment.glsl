#version 330 core

in vec3 Normal;
in vec3 WorldPos;

out vec4 FragColor;
uniform vec3 cameraPos;

// Earth is centred at the origin. When earthRadius is 0 (a2-model-0) the
// simple rim glow is used. The scenes set earthRadius and shellRadius and
// get a glow that depends on how low the line of sight passes over Earth:
// strongest where it grazes the surface, zero where it only touches the top
// of the shell. This is continuous whether the camera is inside or outside
// the shell, so the atmosphere does not pop in or out during the climb.
uniform float earthRadius = 0.0;
uniform float shellRadius = 0.0;

const vec3 atmosphereColor = vec3(0.18, 0.48, 1.0);

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(cameraPos - WorldPos);

    if (earthRadius <= 0.0)
    {
        // Atmospheric glow concentrated at Earth's silhouette.
        float rim = 1.0 - max(dot(N, V), 0.0);
        float alpha = pow(rim, 5.0) * 0.22;
        if (alpha < 0.005)
            discard;
        FragColor = vec4(atmosphereColor, alpha);
        return;
    }

    // Use only the far side of the shell, so every line of sight is
    // counted once (from outside it would otherwise cross the shell twice).
    if (dot(N, V) > 0.0)
        discard;

    // Lowest point of the line of sight between the camera and this fragment.
    vec3 dir = -V;
    float t = clamp(-dot(cameraPos, dir), 0.0, length(WorldPos - cameraPos));
    float lowest = length(cameraPos + t * dir);

    float height = clamp((lowest - earthRadius) / (shellRadius - earthRadius), 0.0, 1.0);
    float glow = pow(1.0 - height, 3.0);
    float alpha = glow * 0.45;
    if (alpha < 0.005)
        discard;

    FragColor = vec4(atmosphereColor, alpha);
}
