#version 330

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outEyeSpaceNormal;

in vec2 texCoords;
in vec3 skyDirection;

uniform int skyMode;           /* 0: original dusk gradient, 1: night sky */
uniform vec3 zenithColor, horizonColor;
uniform float starIntensity;
uniform vec3 moonDirection, moonColor;
uniform float time;
uniform vec3 sunDirection, sunColor, sunsetColor;
uniform float sunVisibility;   /* 0 at night */
uniform float sunsetStrength;  /* horizon glow around sunrise / sunset */

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

vec3 nightSky(vec3 dir) {
    float up = clamp(dir.y, 0.0, 1.0);
    vec3 color = mix(horizonColor, zenithColor, pow(up, 0.45));

    /* stars: one candidate per cell of a 3D grid around the sky sphere */
    vec3 p = dir * 140.0;
    vec3 cell = floor(p);
    float h = hash13(cell);
    if (h > 0.985) {
        vec3 jitter = hash33(cell) - 0.5;
        float d = length(fract(p) - 0.5 - jitter * 0.6);
        float twinkle = 0.65 + 0.35 * sin(time * (1.5 + h * 4.0) + h * 91.0);
        float brightness = (h - 0.985) / 0.015;
        vec3 tint = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.9, 0.75), hash13(cell + 7.0));
        color += tint * smoothstep(0.16, 0.0, d) * twinkle * (0.4 + brightness) *
                 starIntensity * smoothstep(0.02, 0.25, up);
    }

    /* sunrise / sunset glow on the horizon around the sun */
    vec2 flatDir = dir.xz / max(length(dir.xz), 1e-4);
    vec2 flatSun = sunDirection.xz / max(length(sunDirection.xz), 1e-4);
    float towardSun = max(dot(flatDir, flatSun), 0.0);
    color += sunsetColor * sunsetStrength * (0.25 + pow(towardSun, 4.0)) * pow(1.0 - up, 3.0);

    /* sun disc and glow */
    float s = dot(dir, normalize(sunDirection));
    color += sunColor * sunVisibility * (smoothstep(0.9993, 0.9996, s) * 4.0 +
                                         pow(max(s, 0.0), 500.0) * 1.2 +
                                         pow(max(s, 0.0), 24.0) * 0.18);

    /* moon disc, halo and a wide glow */
    float m = dot(dir, normalize(moonDirection));
    color += moonColor * (smoothstep(0.99935, 0.9996, m) * 1.6 +
                          pow(max(m, 0.0), 300.0) * 0.6 +
                          pow(max(m, 0.0), 12.0) * 0.12);
    return color;
}

void main(){
    if (skyMode == 1) {
        outColor = vec4(nightSky(normalize(skyDirection)), 1.0);
    } else {
        outColor = vec4(mix(vec3(0.22, 0.25, 0.58), vec3(0.6, 0.20, 0.1), texCoords.y-0.2), 1) * 0.3;
    }
    outEyeSpaceNormal = vec3(0);
}
