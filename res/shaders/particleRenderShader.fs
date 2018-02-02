#version 330

layout(location = 0) out vec3 outColor;

in vec2 textCoords;
in vec3 normal;

uniform sampler2D particleSampler;

void main(){
    vec3 textColor = texture(particleSampler, textCoords).rgb;
    if(length(textColor) < 0.3){
        discard;
    }
    outColor = textColor;
}
