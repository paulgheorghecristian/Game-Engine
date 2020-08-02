#version 330

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outEyeSpaceNormal;

in vec2 texCoords;

void main(){
    outColor = vec4(mix(vec3(0.52, 0.8, 0.98), vec3(1.0, 0.40, 0.2), texCoords.y-0.2), 1) * 0.3;
    outEyeSpaceNormal = vec3(0);
}
