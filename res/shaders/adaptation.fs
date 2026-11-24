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

/* not a number, or infinite: told from its bits (the driver's compiler may
 * assume numbers are always finite and drop isnan / isinf) */
bool broken(float x) { return (floatBitsToInt(x) & 0x7F800000) == 0x7F800000; }

void main() {
    vec2 scene = textureLod(luminanceSampler, vec2(0.5), luminanceLod).rg;
    float current = exp(scene.r);
    vec4 previous = texture(previousSampler, vec2(0.5));
    /* never carry a broken value on (it would stay for good): start again */
    bool lost = broken(previous.r) || broken(previous.g) || broken(previous.b) || broken(previous.a) ||
                previous.r <= 0.0 || previous.g <= 0.0;
    if (broken(current)) current = lost ? 0.05 : previous.r;
    current = clamp(current, 1e-4, 1000.0);
    /* b: broken pixels seen in the picture lately (fades over a few seconds) */
    float bad = max(lost ? 0.0 : previous.b * 0.99, broken(scene.g) || scene.g > 0.0 ? 1.0 : 0.0);
    if (previous.a <= 0.0 || lost) {
        outColor = vec4(current, current, bad, 1e-3);   /* first frame: settled already */
        return;
    }
    /* a: seconds running. The slow average follows quickly for the first few
     * seconds (the first frames after loading may be dark or half drawn) */
    float elapsed = previous.a;
    float slowRate = mix(2.0, 0.035, smoothstep(1.0, 5.0, elapsed));
    float fast = mix(previous.r, current, 1.0 - exp(-dt * 1.4));
    float slow = mix(previous.g, current, 1.0 - exp(-dt * slowRate));
    outColor = vec4(fast, slow, bad, min(elapsed + dt, 1000.0));
}
