#version 330

/* temporal anti-aliasing: each frame is drawn shifted by a different sub-pixel
 * offset (the projection's jitter); blending it with the previous result,
 * reprojected to where each point was last frame, averages edges, thin wires
 * and grass over time. The history is clamped to the colours around the pixel
 * in this frame, so what moved or changed (fire, particles) does not smear */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D currentSampler;   /* this frame (tone mapped) */
uniform sampler2D historySampler;   /* the previous result */
uniform sampler2D depthSampler;
uniform mat4 reprojection;          /* this frame's clip space -> last frame's (unjittered) */
uniform vec2 texelSize;
uniform float historyValid;         /* 0 on the first frame / after a cut */
uniform float filmGrain;            /* added after the blend (averaging grain would smear it) */
uniform float time;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 grain(vec3 c) {
    if (filmGrain <= 0.0) return c;
    vec2 pixel = textureCoords / texelSize;
    float n = hash12(pixel + fract(time * 17.31) * 731.0) - 0.5;
    n += hash12(pixel * 1.7 + fract(time * 23.7) * 517.0) - 0.5;
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return c + n * filmGrain * (1.0 - 0.6 * luma);
}

vec3 toYCoCg(vec3 c) {
    return vec3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b, -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}

vec3 fromYCoCg(vec3 c) {
    return vec3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}

/* the history sampled with a Catmull-Rom filter (5 taps): bilinear would
 * soften it a little more every frame */
vec3 historyCatmullRom(vec2 uv) {
    vec2 size = 1.0 / texelSize;
    vec2 position = uv * size;
    vec2 centre = floor(position - 0.5) + 0.5;
    vec2 f = position - centre;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);
    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / w12;
    vec2 uv0 = (centre - 1.0) * texelSize, uv3 = (centre + 2.0) * texelSize, uv12 = (centre + offset12) * texelSize;
    vec3 result = texture(historySampler, vec2(uv12.x, uv0.y)).rgb * w12.x * w0.y +
                  texture(historySampler, vec2(uv0.x, uv12.y)).rgb * w0.x * w12.y +
                  texture(historySampler, uv12).rgb * w12.x * w12.y +
                  texture(historySampler, vec2(uv3.x, uv12.y)).rgb * w3.x * w12.y +
                  texture(historySampler, vec2(uv12.x, uv3.y)).rgb * w12.x * w3.y;
    float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    return max(result / weight, vec3(0.0));
}

void main() {
    vec3 current = texture(currentSampler, textureCoords).rgb;
    /* where this point was on screen last frame */
    float depth = texture(depthSampler, textureCoords).r;
    vec4 previous = reprojection * vec4(textureCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec2 previousUV = previous.xy / previous.w * 0.5 + 0.5;
    if (historyValid < 0.5 || any(lessThan(previousUV, vec2(0.0))) || any(greaterThan(previousUV, vec2(1.0)))) {
        outColor = vec4(grain(current), 1.0);
        return;
    }
    /* the colours around this pixel now: the history may not leave their range */
    vec3 lo = vec3(1e9), hi = vec3(-1e9), mean = vec3(0.0);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec3 c = toYCoCg(texture(currentSampler, textureCoords + vec2(x, y) * texelSize).rgb);
            lo = min(lo, c);
            hi = max(hi, c);
            mean += c;
        }
    }
    vec3 history = toYCoCg(historyCatmullRom(previousUV));
    history = clamp(history, lo, hi);
    /* more of this frame where the point moved a lot on screen (less blur) */
    float motion = length((previousUV - textureCoords) / texelSize);
    float feedback = mix(0.88, 0.7, smoothstep(0.5, 6.0, motion));
    vec3 blended = mix(toYCoCg(current), history, feedback);
    /* a light sharpen against the neighbourhood (the jitter averages over a pixel) */
    blended.x += (blended.x - mean.x / 9.0) * 0.22;
    outColor = vec4(grain(max(fromYCoCg(blended), vec3(0.0))), 1.0);
}
