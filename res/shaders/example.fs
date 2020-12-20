#version 330

layout(location = 0) out vec3 outColor;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

uniform sampler2D textureSampler;
uniform Material material;
uniform bool hasTexture;

in vec2 textureCoordinates;

void main(){
    if (!hasTexture) {
        outColor = material.diffuse;
    } else {
        outColor = texture (textureSampler, textureCoordinates).rgb;
    }
}
