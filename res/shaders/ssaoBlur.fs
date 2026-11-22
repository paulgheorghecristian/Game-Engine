#version 330

/* 4x4 box blur: removes the per-pixel noise rotation pattern of the SSAO */

layout(location = 0) out vec4 outAO;

in vec2 textureCoords;

uniform sampler2D aoSampler;
uniform vec2 texelSize;

void main() {
    float sum = 0.0;
    for (int x = -2; x < 2; x++) {
        for (int y = -2; y < 2; y++) {
            sum += texture(aoSampler, textureCoords + (vec2(x, y) + 0.5) * texelSize).r;
        }
    }
    outAO = vec4(vec3(sum / 16.0), 1.0);
}
