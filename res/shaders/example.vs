#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;

uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
uniform mat4 projectionMatrix;

out vec2 textureCoordinates;

void main(){
    textureCoordinates = inTexcoord;

    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(inPosition, 1.0);
}
