#version 330

/* HDR -> display: add bloom, apply exposure and the ACES filmic curve
 * (soft highlight roll-off instead of hard clipping at 1.0) */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D hdrSampler;
uniform sampler2D bloomSamplerNear;  /* half resolution blur */
uniform sampler2D bloomSamplerFar;   /* quarter resolution blur (wider) */
uniform float exposure;
uniform float bloomStrength;
uniform sampler2D flareSampler;    /* the lens flare, when it comes after the black holes */
uniform float flareStrength;       /* 0: already in the HDR image */
uniform float saturation;      /* 1 = untouched */
uniform float gradeStrength;   /* 0 = off: cool shadows / warm highlights and a vignette */

vec3 aces(vec3 x) {
    /* Krzysztof Narkowicz's fit of the ACES filmic curve */
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec3 color = texture(hdrSampler, textureCoords).rgb;
    vec3 bloom = texture(bloomSamplerNear, textureCoords).rgb * 0.6 +
                 texture(bloomSamplerFar, textureCoords).rgb * 0.8;
    color += bloom * bloomStrength;
    if (flareStrength > 0.0) {
        /* a camera effect: on top of whatever the black holes bent */
        vec4 flare = texture(flareSampler, textureCoords);
        color += flare.rgb * flare.a * flareStrength;
    }
    /* the engine's colors are display-referred (no gamma pipeline): tone map in
     * linear light and convert back, so darks keep their contrast and only the
     * bright end is compressed */
    vec3 linear = pow(max(color, vec3(0.0)), vec3(2.2)) * exposure;
    vec3 mapped = pow(aces(linear), vec3(1.0 / 2.2));

    /* colour grade: daylight never looks like fully saturated primaries */
    float luma = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
    mapped = mix(vec3(luma), mapped, saturation);
    if (gradeStrength > 0.0) {
        vec3 tint = mix(vec3(0.96, 0.99, 1.05), vec3(1.04, 1.0, 0.95), smoothstep(0.1, 0.8, luma));
        mapped *= mix(vec3(1.0), tint, gradeStrength);
        vec2 v = textureCoords - 0.5;
        mapped *= 1.0 - gradeStrength * 0.22 * smoothstep(0.25, 0.85, dot(v, v) * 2.0);
    }
    outColor = vec4(clamp(mapped, 0.0, 1.0), 1.0);
}
