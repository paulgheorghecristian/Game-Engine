#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in mat4 modelMatrix;

uniform mat4 viewMatrix, projectionMatrix;

out vec3 eyeSpaceNormal;
out vec2 textureCoords;
flat out mat3 fromTangentToModelSpace;
flat out mat4 frag_modelMatrix;

void main(){
	eyeSpaceNormal = (mat3(viewMatrix) * mat3(modelMatrix) * inNormal).xyz;
	textureCoords = inTexcoord;
	frag_modelMatrix = modelMatrix;

	vec3 normalizedNormal = inNormal;
	vec3 normalizedTangent = inTangent;
	vec3 normalizedBiTangent = inBiTangent;

	fromTangentToModelSpace = mat3 (normalizedTangent.x, normalizedTangent.y, normalizedTangent.z,
                                    normalizedBiTangent.x, normalizedBiTangent.y, normalizedBiTangent.z,
                                    normalizedNormal.x, normalizedNormal.y, normalizedNormal.z);

	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(inPosition,1);
}
