#version 330

layout(location = 0) out vec4 outColor;

in vec2 textCoords;
in vec2 previousTextCoords;
in vec3 normal;
in float blendFactor;

uniform sampler2D particleSampler;

void main(){
    vec4 textColor = texture(particleSampler, textCoords);
    vec4 prevTextColor = texture(particleSampler, previousTextCoords);
    /*if(length(textColor) < 0.3){
        discard;
    }*/
    outColor = mix (textColor, prevTextColor, blendFactor);
}
