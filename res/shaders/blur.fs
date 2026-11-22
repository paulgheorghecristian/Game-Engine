#version 330

/* separable 9-tap Gaussian (direction = texel step along x or y) */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D sourceSampler;
uniform vec2 direction;

void main() {
    const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 sum = texture(sourceSampler, textureCoords).rgb * weights[0];
    for (int i = 1; i < 5; i++) {
        sum += texture(sourceSampler, textureCoords + direction * float(i)).rgb * weights[i];
        sum += texture(sourceSampler, textureCoords - direction * float(i)).rgb * weights[i];
    }
    outColor = vec4(sum, 1.0);
}
