#version 330

in vec2 textureCoords;
out vec4 outColor;

uniform sampler2D colorSampler;

void main() {
    outColor = vec4(texture(colorSampler, textureCoords).rgb, 1.0);
}
