#version 430

layout(location = 0) out vec3 outColor;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

uniform Material material;

void main(){
    outColor = material.ambient;
}
