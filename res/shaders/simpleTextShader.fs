#version 330

out vec4 out_color;

in vec2 text_coords;

uniform vec3 color;
uniform sampler2D fontAtlas;
uniform float d1 = 0.45;
uniform float d2 = 0.1;

void main(){
	vec4 letter_pixel = texture(fontAtlas, text_coords);

	float amt = 1.0 - smoothstep(d1, d1+d2, 1.0-letter_pixel.a);
	out_color = vec4 (color, amt);
}
