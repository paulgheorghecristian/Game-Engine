#version 330

/* The bounce light (Rendering/BounceLight) smoothed along one direction (run twice: across, then
 * down): 9 taps, each counted less the further its depth or its normal is from the pixel's, so the
 * light does not run over an edge onto what is in front or behind */

layout(location = 0) out vec4 outBounce;

in vec2 textureCoords;

uniform sampler2D bounceSampler;
uniform sampler2D depthSampler;
uniform sampler2D normalSampler;
uniform mat4 inverseProjectionMatrix;
uniform vec2 direction;             /* one texel of the bounce target along x or y */

float viewDepth(vec2 uv) {
    float d = textureLod(depthSampler, uv, 0.0).r;
    vec4 p = inverseProjectionMatrix * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return -p.z / p.w;
}

void main() {
    const float WEIGHTS[5] = float[5](0.2270, 0.1945, 0.1216, 0.0541, 0.0162);
    float z = viewDepth(textureCoords);
    vec3 n = texture(normalSampler, textureCoords).xyz * 2.0 - 1.0;
    vec3 sum = vec3(0.0);
    float total = 0.0;
    for (int i = -4; i <= 4; i++) {
        vec2 uv = textureCoords + direction * float(i) * 1.5;
        float w = WEIGHTS[abs(i)];
        w *= exp(-abs(viewDepth(uv) - z) / (0.02 * z + 0.5));
        w *= pow(max(dot(n, textureLod(normalSampler, uv, 0.0).xyz * 2.0 - 1.0), 0.0), 8.0);
        sum += textureLod(bounceSampler, uv, 0.0).rgb * w;
        total += w;
    }
    outBounce = vec4(sum / max(total, 1e-4), 1.0);
}
