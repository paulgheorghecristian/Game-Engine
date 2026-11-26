#version 330

/* The glow of the black holes' own light (BlackHoleGlow): their light, already times each hole's
 * bloom, spread at several widths and added. A lens spreads light into a bright core and faint wide
 * wings; each wider level gets less weight, but it also gathers more of a bright source's light, so
 * the halo reaches further the brighter the light is (Starless: radius as the cube root of the
 * brightness) */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D glowSampler;

/* one level of the mip chain, smoothed: four taps half a texel of that level apart (one tap shows
 * the level's square texels) */
vec3 level(float lod) {
    vec2 texel = exp2(lod) / vec2(textureSize(glowSampler, 0));
    return (textureLod(glowSampler, textureCoords + texel * vec2(-0.5, -0.5), lod).rgb +
            textureLod(glowSampler, textureCoords + texel * vec2(0.5, -0.5), lod).rgb +
            textureLod(glowSampler, textureCoords + texel * vec2(-0.5, 0.5), lod).rgb +
            textureLod(glowSampler, textureCoords + texel * vec2(0.5, 0.5), lod).rgb) * 0.25;
}

void main() {
    /* (summing to 1: bloom 1 adds the light once more, spread; most of it in the wide levels: a halo
     * round the light rather than the light itself made brighter) */
    const float WEIGHTS[7] = float[7](0.08, 0.12, 0.16, 0.18, 0.17, 0.15, 0.14);
    vec3 glow = vec3(0.0);
    for (int i = 0; i < 7; i++) {
        glow += level(float(i + 1)) * WEIGHTS[i];
    }
    outColor = vec4(glow, 1.0);
}
