#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in mat4 modelMatrix;

uniform mat4 viewMatrix, projectionMatrix;

void main(){
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(inPosition,1);
}
