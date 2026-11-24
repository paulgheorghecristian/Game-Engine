#version 330

/* exposure adaptation, step 1: the scene's log luminance, downsampled (its
 * mipmaps then average it into one pixel) */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D hdrSampler;

/* not a number, or infinite: told from its bits (the driver's compiler may
 * assume numbers are always finite and drop isnan / isinf) */
bool broken(float x) { return (floatBitsToInt(x) & 0x7F800000) == 0x7F800000; }

void main() {
    /* a few taps across this texel's footprint of the full image */
    vec2 texel = 1.0 / vec2(textureSize(hdrSampler, 0));
    vec3 c = vec3(0.0);
    float bad = 0.0;
    for (int i = 0; i < 4; i++) {
        vec2 o = vec2(float(i & 1) - 0.5, float(i >> 1) - 0.5) * texel * 6.0;
        vec3 tap = texture(hdrSampler, textureCoords + o).rgb;
        /* a broken pixel (not a number, or infinite) must not get in: averaged
         * into the one adapted value it never leaves it, and the picture
         * stays black from then on */
        if (broken(tap.r) || broken(tap.g) || broken(tap.b)) {
            tap = vec3(0.0);
            bad += 0.25;
        }
        c += clamp(tap, vec3(0.0), vec3(1000.0));
    }
    float luminance = dot(c * 0.25, vec3(0.2126, 0.7152, 0.0722));
    /* g: how much of it was broken (for whoever looks for where it comes from) */
    outColor = vec4(log(luminance + 1e-3), bad, 0.0, 1.0);
}
