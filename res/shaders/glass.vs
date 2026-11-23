#version 330

/* glass (forward, after the scene is composited): see glass.fs */

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

uniform mat4 modelMatrix, viewMatrix, projectionMatrix;

out vec3 worldPosition;
out vec3 worldNormal;

void main() {
    worldPosition = (modelMatrix * vec4(inPosition, 1)).xyz;
    worldNormal = transpose(inverse(mat3(modelMatrix))) * inNormal;
    gl_Position = projectionMatrix * viewMatrix * vec4(worldPosition, 1);
}
