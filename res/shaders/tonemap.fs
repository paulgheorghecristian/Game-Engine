#version 330

/* HDR -> display: add bloom, apply exposure and the ACES filmic curve
 * (soft highlight roll-off instead of hard clipping at 1.0) */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D hdrSampler;
uniform sampler2D bloomSamplerNear;  /* half resolution blur */
uniform sampler2D bloomSamplerFar;   /* quarter resolution blur (wider) */
uniform float exposure;
uniform float bloomStrength;
uniform sampler2D flareSampler;    /* the lens flare, when it comes after the black holes */
uniform float flareStrength;       /* 0: already in the HDR image */

/* heat haze: columns of hot air rising from fires bend what is seen through
 * them. Each: base position, and (radius, height, strength) */
const int MAX_HEAT = 8;
uniform int heatCount;
uniform vec3 heatPositions[MAX_HEAT];
uniform vec3 heatParams[MAX_HEAT];
uniform sampler2D depthSampler;
uniform mat4 inverseProjectionMatrix, inverseViewMatrix;
uniform float time;

/* the camera's lens: dust and smudges lit by the bloom, chromatic aberration
 * towards the edges, film grain */
uniform float lensDirt, chromaticAberration, filmGrain;
/* exposure adaptation: r = the eye's adapted luminance, g = this place's
 * usual one; exposure scales by (usual / adapted)^autoExposure */
uniform sampler2D adaptationSampler;
uniform float autoExposure;
uniform vec2 screenSize;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}

/* a fixed pattern of grime on the lens (procedural: smudges and specks) */
float lensDirtAt(vec2 uv) {
    vec2 p = uv * vec2(screenSize.x / screenSize.y, 1.0);
    float smudge = noise2(p * 3.0) * 0.6 + noise2(p * 7.0 + 4.0) * 0.4;
    smudge = smoothstep(0.45, 0.85, smudge);
    float specks = 0.0;
    for (int k = 0; k < 2; k++) {
        vec2 q = p * (14.0 + 16.0 * float(k)) + float(k) * 13.0;
        vec2 cell = floor(q);
        float h = hash12(cell);
        if (h > 0.82) {
            vec2 c = vec2(hash12(cell + 1.7), hash12(cell + 9.1)) * 0.6 + 0.2;
            specks += smoothstep(0.35, 0.0, length(fract(q) - c)) * (h - 0.82) * 5.5;
        }
    }
    /* grime gathers towards the rim of the lens */
    float rim = smoothstep(0.15, 0.75, length(uv - 0.5));
    return (smudge * 0.6 + specks) * (0.35 + 0.65 * rim);
}

vec2 heatOffset(vec2 uv) {
    if (heatCount == 0) return vec2(0.0);
    float depth = texture(depthSampler, uv).r;
    vec4 view = inverseProjectionMatrix * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    view /= view.w;
    float sceneDistance = depth >= 1.0 ? 1e9 : length(view.xyz);
    vec3 o = inverseViewMatrix[3].xyz;
    vec3 d = normalize(mat3(inverseViewMatrix) * view.xyz);
    vec2 offset = vec2(0.0);
    for (int i = 0; i < heatCount; i++) {
        float radius = heatParams[i].x, height = heatParams[i].y, strength = heatParams[i].z;
        /* where the view ray passes closest to the column's vertical axis */
        vec3 w0 = o - heatPositions[i];
        float b = d.y, dd = dot(d, w0), e = w0.y;
        float denom = max(1.0 - b * b, 1e-4);
        float t = (b * e - dd) / denom;
        if (t <= 1.0 || t >= sceneDistance) continue;     /* behind us, or behind what is seen */
        float s = clamp((e - b * dd) / denom, 0.0, height);
        float dist = length(o + d * t - heatPositions[i] - vec3(0.0, s, 0.0));
        float h = s / height;
        float weight = (1.0 - smoothstep(0.0, radius, dist)) * smoothstep(0.0, 0.08, h) * (1.0 - h) * (1.0 - h);
        if (weight <= 0.0) continue;
        /* the air's waves, rising */
        vec3 p = o + d * t;
        vec2 wobble = vec2(sin(p.y * 0.55 - time * 7.0 + p.x * 0.3) + 0.5 * sin(p.y * 1.3 - time * 11.0 + p.z),
                           cos(p.y * 0.45 - time * 6.0 + p.z * 0.35));
        offset += wobble * weight * strength * (2.2 / t);
    }
    return offset * 0.01;
}
uniform float saturation;      /* 1 = untouched */
uniform float gradeStrength;   /* 0 = off: cool shadows / warm highlights and a vignette */

vec3 aces(vec3 x) {
    /* Krzysztof Narkowicz's fit of the ACES filmic curve */
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec2 uv = textureCoords + heatOffset(textureCoords);
    vec3 color = texture(hdrSampler, uv).rgb;
    if (chromaticAberration > 0.0) {
        /* the lens bends red and blue a little differently, more towards the edges */
        vec2 fromCentre = uv - 0.5;
        vec2 shift = fromCentre * dot(fromCentre, fromCentre) * chromaticAberration * 0.02;
        color.r = texture(hdrSampler, uv + shift).r;
        color.b = texture(hdrSampler, uv - shift).b;
    }
    vec3 bloomFar = texture(bloomSamplerFar, uv).rgb;
    vec3 bloom = texture(bloomSamplerNear, uv).rgb * 0.6 + bloomFar * 0.8;
    color += bloom * bloomStrength;
    if (lensDirt > 0.0) {
        /* bright light scattered by the grime on the lens */
        color += bloomFar * lensDirtAt(textureCoords) * lensDirt * 1.6 * max(bloomStrength, 0.3);
    }
    if (flareStrength > 0.0) {
        /* a camera effect: on top of whatever the black holes bent */
        vec4 flare = texture(flareSampler, textureCoords);
        color += flare.rgb * flare.a * flareStrength;
    }
    /* the engine's colors are display-referred (no gamma pipeline): tone map in
     * linear light and convert back, so darks keep their contrast and only the
     * bright end is compressed */
    float adaptedExposure = exposure;
    if (autoExposure > 0.0) {
        vec2 adapted = texture(adaptationSampler, vec2(0.5)).rg;
        adaptedExposure *= clamp(pow(max(adapted.g, 1e-4) / max(adapted.r, 1e-4), autoExposure), 0.45, 2.2);
    }
    vec3 linear = pow(max(color, vec3(0.0)), vec3(2.2)) * adaptedExposure;
    vec3 mapped = pow(aces(linear), vec3(1.0 / 2.2));

    /* colour grade: daylight never looks like fully saturated primaries */
    float luma = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
    mapped = mix(vec3(luma), mapped, saturation);
    if (gradeStrength > 0.0) {
        vec3 tint = mix(vec3(0.96, 0.99, 1.05), vec3(1.04, 1.0, 0.95), smoothstep(0.1, 0.8, luma));
        mapped *= mix(vec3(1.0), tint, gradeStrength);
        vec2 v = textureCoords - 0.5;
        mapped *= 1.0 - gradeStrength * 0.22 * smoothstep(0.25, 0.85, dot(v, v) * 2.0);
    }
    if (filmGrain > 0.0) {
        /* grain, changing every frame, stronger in the shadows */
        float n = hash12(textureCoords * screenSize + fract(time * 17.31) * 731.0) - 0.5;
        n += hash12(textureCoords * screenSize * 1.7 + fract(time * 23.7) * 517.0) - 0.5;
        mapped += n * filmGrain * (1.0 - 0.6 * luma);
    }
    outColor = vec4(clamp(mapped, 0.0, 1.0), 1.0);
}
