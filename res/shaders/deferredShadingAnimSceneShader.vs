#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in vec4 inBoneIndices;

uniform mat4 modelMatrix, viewMatrix, projectionMatrix;

out vec3 eyeSpaceNormal;
out vec2 textureCoords;
out mat3 fromTangentToModelSpace;
out vec4 we;

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];

void main(){
	eyeSpaceNormal = (mat3(viewMatrix) * mat3(modelMatrix) * inNormal).xyz;
	textureCoords = inTexcoord;
	we = inBoneWeights;

	mat4 BMatrix = gBones[int(inBoneIndices[0])] * inBoneWeights[0];
          BMatrix += gBones[int(inBoneIndices[1])] * inBoneWeights[1];
          BMatrix += gBones[int(inBoneIndices[2])] * inBoneWeights[2];
          BMatrix += gBones[int(inBoneIndices[3])] * inBoneWeights[3];

	vec3 normalizedNormal = inNormal;
	vec3 normalizedTangent = inTangent;
	vec3 normalizedBiTangent = inBiTangent;

	// re-orthogonalize T with respect to N
	vec3 T = normalize(inTangent - dot(inTangent, inNormal) * inNormal);
	// then retrieve perpendicular vector B with the cross product of T and N
	normalizedBiTangent = cross(normalizedNormal, T);

	fromTangentToModelSpace = mat3 (normalizedTangent.x, normalizedTangent.y, normalizedTangent.z,
                                    normalizedBiTangent.x, normalizedBiTangent.y, normalizedBiTangent.z,
                                    normalizedNormal.x, normalizedNormal.y, normalizedNormal.z);

	gl_Position = projectionMatrix * viewMatrix * modelMatrix * BMatrix * vec4(inPosition,1);
}
