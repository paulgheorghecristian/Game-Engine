#version 330

layout(location = 0) out vec3 outColor;

in vec2 textureCoords;

uniform float pixelWidth;
uniform float pixelHeight;
uniform sampler2D textureSampler;

void main(){
    float weights[] = {0.0093, 0.028002, 0.065984, 0.121703, 0.175713,
                        0.198596, 0.175713, 0.121703, 0.065984, 0.028002, 0.0093};
    vec2 coords;
    vec3 pixel = vec3(0);

    coords = vec2(textureCoords.x + pixelWidth*-6, textureCoords.y);

    for (int i = 0; i < 11; i++, coords.x += pixelWidth) {
        if (coords.x < 0) {
            continue;
        }
        pixel += weights[i] * texture(textureSampler, coords).xyz;
        if (coords.x >= 1) {
            break;
        }
    }

    outColor = pixel;
}