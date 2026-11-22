#version 330

in vec3 worldPosition;
in vec2 textureCoords;

struct Material {
    bool isBlackAlpha;
};

uniform Material material;
uniform sampler2D textureSampler;
uniform bool hasTexture;
uniform vec3 lightPosition;
uniform float farPlane;

void main() {
    /* cut-out leaves cast dappled shadows */
    if (hasTexture && material.isBlackAlpha) {
        vec3 texel = texture(textureSampler, textureCoords).rgb;
        if ((texel.r + texel.g + texel.b) / 3.0 <= 0.1) {
            discard;
        }
    }
    /* store linear distance so the lookup works the same for all six faces */
    gl_FragDepth = length(worldPosition - lightPosition) / farPlane;
}
