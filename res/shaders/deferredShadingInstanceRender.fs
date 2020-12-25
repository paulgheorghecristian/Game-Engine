#version 330

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outEyeSpaceNormal;
layout(location = 2) out vec3 outRoughness;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    float normalMapStrength;
    bool isBlackAlpha;
};

uniform sampler2D textureSampler;
uniform sampler2D normalMapSampler;
uniform sampler2D roughnessSampler;
uniform Material material;
uniform bool hasTexture;
uniform bool hasNormalMap;
uniform bool hasRoughness;
uniform mat4 viewMatrix;

in vec3 eyeSpaceNormal;
in vec2 textureCoords;
in mat3 fromTangentToModelSpace;
in mat4 frag_modelMatrix;

void main(){
    if (hasTexture){
        vec3 color = texture (textureSampler, textureCoords).rgb;
        if (material.isBlackAlpha == true && (color.r+color.g+color.b)/3.0f < 0.1f)
            discard;
        outColor = vec4 (color * material.diffuse, 1.0);
    } else {
        outColor = vec4 (material.diffuse, 1.0);
    }

    int frontCond = -(1 - int(gl_FrontFacing)*2);

    if (!hasNormalMap) {
        outEyeSpaceNormal = (normalize(eyeSpaceNormal * frontCond) + vec3(1)) * 0.5;
	} else {
        vec3 normalMapNormal = texture (normalMapSampler, textureCoords).xyz;
        normalMapNormal = normalMapNormal * 2.0 - 1.0;
        normalMapNormal.xy *= material.normalMapStrength;

        outEyeSpaceNormal = (normalize(mat3(viewMatrix) * mat3(frag_modelMatrix) * fromTangentToModelSpace * normalMapNormal * frontCond) + vec3(1.0)) * 0.5;
	}

	if (!hasRoughness) {
        outRoughness = vec3(0.5);
	} else {
        outRoughness = texture(roughnessSampler, textureCoords).xyz;
	}
}
