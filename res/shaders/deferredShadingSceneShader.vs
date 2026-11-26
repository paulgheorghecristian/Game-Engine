#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;

uniform mat4 modelMatrix, viewMatrix, projectionMatrix;

out vec3 eyeSpaceNormal;
out vec2 textureCoords;
out mat3 fromTangentToModelSpace;
out vec3 worldPosition;
out vec3 worldNormal; /* inverse-transpose: correct for non-uniform scale */
out vec3 objectPosition, objectNormal; /* the mesh's own space (textures projected by tile size) */

void main(){
	eyeSpaceNormal = (mat3(viewMatrix) * mat3(modelMatrix) * inNormal).xyz;
	textureCoords = inTexcoord;

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

	objectPosition = inPosition;
	objectNormal = inNormal;
	worldPosition = (modelMatrix * vec4(inPosition, 1)).xyz;
	worldNormal = transpose(inverse(mat3(modelMatrix))) * inNormal;
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(inPosition,1);
}
