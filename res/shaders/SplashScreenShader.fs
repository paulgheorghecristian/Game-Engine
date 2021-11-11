#version 330

out vec4 outColor;

uniform sampler2D splashScreenSampler;
in vec2 textureCoords;

void main(){
	vec3 splashScreen = texture(splashScreenSampler, textureCoords).rgb;
	outColor = vec4(splashScreen, 1);
}

