#version 330

/* G-buffer output for grass blades: colored like the terrain's grass under
 * them (same textures, scales and patchiness as the terrain shader) */

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outEyeSpaceNormal;
layout(location = 2) out vec3 outRoughness;

in vec3 eyeSpaceNormal;
in vec3 worldPosition;
in float bladeT;
flat in float bladeShade;
in float farBlend;

uniform bool pbrEnabled;
uniform bool hasTerrainTextures;
uniform sampler2D grassSampler, grass2Sampler;
uniform float terrainTileSize, terrainShoreHeight, terrainVariation;
uniform float albedoScale;      /* match the terrain material's diffuse */

float terrainHash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float terrainNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(terrainHash(i), terrainHash(i + vec2(1, 0)), u.x),
               mix(terrainHash(i + vec2(0, 1)), terrainHash(i + vec2(1, 1)), u.x), u.y);
}

void main() {
    vec3 grass = vec3(0.3, 0.45, 0.15);
    if (hasTerrainTextures) {
        vec2 uv = worldPosition.xz / terrainTileSize;
        float macro = terrainNoise(worldPosition.xz * 0.0035) * 0.65 + terrainNoise(worldPosition.xz * 0.011) * 0.35;
        vec3 grassNear = texture(grass2Sampler, uv).rgb;
        vec3 grassAlt = texture(grassSampler, uv * 1.37 + vec2(0.31, 0.17)).rgb;
        vec3 grassFar = texture(grass2Sampler, uv * 0.23 + vec2(0.5)).rgb;
        grass = mix(mix(grassNear, grassAlt, smoothstep(0.35, 0.7, macro)), grassFar, 0.35);
        if (terrainVariation > 0.0) {
            float dryNoise = terrainNoise(worldPosition.xz * 0.0019 + vec2(3.7, 1.3)) * 0.65 +
                             terrainNoise(worldPosition.xz * 0.0071 + vec2(9.1, 4.2)) * 0.35;
            float altitude = smoothstep(terrainShoreHeight + 90.0, terrainShoreHeight + 420.0, worldPosition.y);
            float dry = clamp(smoothstep(0.48, 0.78, dryNoise) * 0.8 + altitude * 0.45, 0.0, 1.0) * terrainVariation;
            float lush = (1.0 - smoothstep(0.2, 0.45, dryNoise)) * (1.0 - altitude) * terrainVariation;
            float g = dot(grass, vec3(0.3333));
            grass = mix(grass, vec3(g * 1.25, g * 1.08, g * 0.62), dry * 0.7);
            grass = mix(grass, grass * vec3(0.78, 0.9, 0.72), lush * 0.6);
        }
        grass *= mix(0.82, 1.12, macro);
    }
    /* darker at the root (shaded by the other blades), lighter, yellower tips */
    vec3 albedo = grass * mix(0.6, 1.12, bladeT) * bladeShade;
    albedo = mix(albedo, albedo * vec3(1.08, 1.05, 0.8), bladeT * 0.5);
    /* far away the blades take the plain ground color, so the edge of the
     * grass field blends into the terrain instead of showing as a darker band */
    albedo = mix(albedo, grass, farBlend);

    outColor = vec4(albedo * albedoScale, 1.0);
    outEyeSpaceNormal = (normalize(eyeSpaceNormal) + vec3(1.0)) * 0.5;
    outRoughness = pbrEnabled ? vec3(0.85, 0.0, 0.0) : vec3(0.06);
}
