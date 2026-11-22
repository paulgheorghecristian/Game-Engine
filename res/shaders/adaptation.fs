#version 330

/* exposure adaptation, step 2: the eye's adapted luminance, following the
 * scene's average over time. r: fast (the eye, ~1.5 s), g: slow (what this
 * place normally looks like, ~30 s), a: seconds since it started (0: not yet) */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D luminanceSampler;   /* log luminance, mipmapped */
uniform sampler2D previousSampler;    /* last frame's adapted values (1x1) */
uniform float luminanceLod;           /* its 1x1 mip level */
uniform float dt;

void main() {
    float current = exp(textureLod(luminanceSampler, vec2(0.5), luminanceLod).r);
    vec4 previous = texture(previousSampler, vec2(0.5));
    if (previous.a <= 0.0) {
        outColor = vec4(current, current, 0.0, 1e-3);   /* first frame: settled already */
        return;
    }
    /* a: seconds running. The slow average follows quickly for the first few
     * seconds (the first frames after loading may be dark or half drawn) */
    float elapsed = previous.a;
    float slowRate = mix(2.0, 0.035, smoothstep(1.0, 5.0, elapsed));
    float fast = mix(previous.r, current, 1.0 - exp(-dt * 1.4));
    float slow = mix(previous.g, current, 1.0 - exp(-dt * slowRate));
    outColor = vec4(fast, slow, 0.0, min(elapsed + dt, 1000.0));
}
