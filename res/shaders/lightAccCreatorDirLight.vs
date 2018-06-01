#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexture;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform vec3 lightDirection;

out vec3 viewRay;
flat out vec3 lightDirectionEyeSpace;

void main(){
    viewRay = (inverse(projectionMatrix) * vec4(inPosition, 1.0)).xyz;
    lightDirectionEyeSpace = normalize(mat3(viewMatrix) * lightDirection);
    gl_Position = vec4(inPosition, 1.0);
}
