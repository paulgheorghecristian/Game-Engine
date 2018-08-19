#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexture;

uniform mat4 projectionMatrix;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform vec3 lightPosition;

out vec3 viewRay;
flat out vec3 lightPositionEyeSpace;

void main(){
    vec4 viewPosition;

    viewPosition = viewMatrix * modelMatrix * vec4(inPosition, 1.0);
    viewRay = viewPosition.xyz;
    lightPositionEyeSpace = (viewMatrix * vec4(lightPosition, 1.0)).xyz;

    gl_Position = projectionMatrix * viewPosition;
}
