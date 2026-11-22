#version 330

/* exposure adaptation, step 1: the scene's log luminance, downsampled (its
 * mipmaps then average it into one pixel) */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D hdrSampler;

void main() {
    /* a few taps across this texel's footprint of the full image */
    vec2 texel = 1.0 / vec2(textureSize(hdrSampler, 0));
    vec3 c = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        vec2 o = vec2(float(i & 1) - 0.5, float(i >> 1) - 0.5) * texel * 6.0;
        c += texture(hdrSampler, textureCoords + o).rgb;
    }
    float luminance = dot(c * 0.25, vec3(0.2126, 0.7152, 0.0722));
    outColor = vec4(log(luminance + 1e-3), 0.0, 0.0, 1.0);
}
