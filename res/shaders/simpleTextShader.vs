#version 330

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texture;

uniform mat4 projectionMatrix;
uniform mat4 modelMatrix;

out vec2 text_coords;

void main(){
	text_coords = in_texture;
	gl_Position = projectionMatrix * modelMatrix * vec4(in_position, 1.0);
}
