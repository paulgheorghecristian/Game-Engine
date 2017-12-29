#version 330

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outEyeSpaceNormal;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

uniform sampler2D textureSampler;
uniform sampler2D normalMapSampler;
uniform Material material;
uniform bool hasTexture;
uniform bool hasNormalMap;
uniform mat4 viewMatrix, modelMatrix;

in vec3 eyeSpaceNormal;
in vec2 textureCoords;
flat in mat3 fromTangentToModelSpace;

void main(){

    if (hasTexture){
        vec3 color = texture (textureSampler, textureCoords).rgb;
        outColor = color * material.ambient;
    } else {
        outColor = material.ambient;
	}

    int frontCond = -(1 - int(gl_FrontFacing)*2);

    if (!hasNormalMap) {
        outEyeSpaceNormal = (normalize(eyeSpaceNormal * frontCond) + vec3(1)) * 0.5;
	} else {
        vec3 normalMapNormal = texture (normalMapSampler, textureCoords).xyz;
        normalMapNormal = normalMapNormal * 2.0 - 1.0;

        outEyeSpaceNormal = (normalize(mat3(viewMatrix) * mat3(modelMatrix) * fromTangentToModelSpace * normalMapNormal * frontCond) + vec3(1.0)) * 0.5;
	}
}
