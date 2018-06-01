#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords;

out vec2 texCoords;

uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
uniform mat4 projectionMatrix;

void main(){
    texCoords = inTexCoords;
    gl_Position = projectionMatrix * mat4(mat3(viewMatrix)) * modelMatrix *  vec4(inPosition, 1);
}
