#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexture;

uniform mat4 projectionMatrix;

out vec3 viewRay;

void main(){
    viewRay = (inverse(projectionMatrix) * vec4(inPosition, 1.0)).xyz;
	gl_Position = vec4(inPosition, 1.0);
}
