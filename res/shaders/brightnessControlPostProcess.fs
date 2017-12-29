#version 330

layout(location = 0) out vec3 outColor;

in vec2 textureCoords;

uniform float pixelWidth;
uniform float pixelHeight;
uniform sampler2D textureSampler;

void main(){
    vec3 pixelColor = texture (textureSampler, textureCoords).xyz;
    float brightness = (pixelColor.r * 0.2126)+
                       (pixelColor.g * 0.7152)+
                       (pixelColor.b * 0.0722);
    outColor = pixelColor * pow (brightness, 10.0);
}
