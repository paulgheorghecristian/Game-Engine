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
/* value noise in 3D (the third axis is time: shapes change, not slide) */
float dustHash(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}
float dustNoise(vec3 x) {
    vec3 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(dustHash(i), dustHash(i + vec3(1, 0, 0)), f.x),
                   mix(dustHash(i + vec3(0, 1, 0)), dustHash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(dustHash(i + vec3(0, 0, 1)), dustHash(i + vec3(1, 0, 1)), f.x),
                   mix(dustHash(i + vec3(0, 1, 1)), dustHash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

/* the storm (Core/Aftermath): its dust covering the picture, rain streaking
 * past and water on the lens */
uniform float dustVeil;        /* 0..1: 1 = nothing but dust */
uniform vec3 dustColor;
uniform float rainAmount, lensWater;
uniform float lightning;       /* 0..1: a flash in the dust (blinding) */
uniform vec3 bolt;             /* where the flash is: screen x, a seed, its height (0..1) */

/* drops on the lens: each bends the picture like a little lens (flipped,
 * squeezed), and they come and go. Returns the uv offset; rim: their edges */
vec2 lensDrops(vec2 uv, out float rim) {
    vec2 aspect = vec2(screenSize.x / screenSize.y, 1.0);
    vec2 offset = vec2(0.0);
    rim = 0.0;
    for (int layer = 0; layer < 2; layer++) {
        float scale = layer == 0 ? 6.0 : 11.0;
        vec2 p = uv * aspect * scale + float(layer) * 7.3;
        vec2 cell = floor(p);
        float h = hash12(cell + float(layer) * 17.0);
        if (h > lensWater * 0.55) continue;
        /* hits, stays, runs off: each drop's own cycle */
        float life = fract(time * (0.12 + 0.2 * hash12(cell + 2.3)) + hash12(cell + 4.1));
        float there = smoothstep(0.0, 0.04, life) * (1.0 - smoothstep(0.7, 1.0, life));
        vec2 c = vec2(hash12(cell + 3.1), hash12(cell + 5.7)) * 0.5 + 0.25;
        c.x += life * 0.15;                       /* blown sideways */
        float r = 0.12 + 0.2 * hash12(cell + 9.3);
        vec2 d = (fract(p) - c) / r;
        float dist = length(d);
        if (dist < 1.0) {
            float bulge = sqrt(1.0 - dist * dist);
            offset -= d * r / scale / aspect * 2.2 * bulge * there;
            rim += smoothstep(0.65, 1.0, dist) * there;
        }
    }
    return offset;
}

/* rain driven by the gale: thin slanted streaks rushing past */
float rainStreaks(vec2 uv) {
    vec2 p = uv * vec2(screenSize.x / screenSize.y, 1.0);
    float s = 0.0;
    for (int k = 0; k < 3; k++) {
        /* three depths: near streaks bigger, faster and fewer */
        float columns = k == 0 ? 45.0 : (k == 1 ? 85.0 : 150.0);
        float slant = 0.32 + 0.12 * sin(time * 0.6 + float(k) * 1.7) + 0.05 * sin(time * 1.9 + float(k));   /* gusts */
        float qx = (p.x + p.y * slant) * columns;
        float column = floor(qx);
        float c1 = hash12(vec2(column, 3.1 + float(k) * 17.0)), c2 = hash12(vec2(column, 7.7 + float(k) * 13.0));
        /* each column its own speed and phase */
        float speed = (9.0 + 4.0 * float(k)) * (0.7 + 0.6 * c1);
        float qy = p.y * (2.2 + float(k)) * (0.8 + 0.4 * c2) + time * speed + c2 * 10.0;
        float row = floor(qy), y = fract(qy);
        vec2 id = vec2(column, row) + float(k) * 31.0;
        float h = hash12(id), length_ = 0.25 + 0.55 * hash12(id + 1.7);
        if (h < 0.8) continue;
        /* its place in the column, and its own slight tilt */
        float centre = 0.2 + 0.6 * hash12(id + 4.3) + (y - 0.5) * (hash12(id + 9.1) - 0.5) * 0.9;
        float width = k == 0 ? 0.09 : 0.12;
        float line = smoothstep(width, 0.0, abs(fract(qx) - centre));
        float segment = smoothstep(0.0, 0.08, y) * (1.0 - smoothstep(length_ - 0.1, length_, y));
        s += line * segment * (0.35 + 0.65 * hash12(id + 6.6)) * (k == 0 ? 1.0 : (k == 1 ? 0.7 : 0.45));
    }
    return s;
}

uniform float saturation;      /* 1 = untouched */
uniform float gradeStrength;   /* 0 = off: cool shadows / warm highlights and a vignette */

vec3 aces(vec3 x) {
    /* Krzysztof Narkowicz's fit of the ACES filmic curve */
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec2 uv = textureCoords + heatOffset(textureCoords);
    float dropRim = 0.0;
    if (lensWater > 0.0) {
        uv += lensDrops(textureCoords, dropRim);
    }
    vec3 color = texture(hdrSampler, uv).rgb;
    if (chromaticAberration > 0.0) {
        /* the lens bends red and blue a little differently, more towards the edges */
        vec2 fromCentre = uv - 0.5;
        vec2 shift = fromCentre * dot(fromCentre, fromCentre) * chromaticAberration * 0.02;
        vec3 outer = texture(hdrSampler, uv + shift).rgb, inner = texture(hdrSampler, uv - shift).rgb;
        color.r = outer.r;
        color.b = inner.b;
        /* green spread between them too: with only red and blue moved, a small
         * bright spark split into red, GREEN and blue dots */
        color.g = color.g * 0.5 + (outer.g + inner.g) * 0.25;
    }
    vec3 bloomFar = texture(bloomSamplerFar, uv).rgb;
    vec3 bloom = texture(bloomSamplerNear, uv).rgb * 0.6 + bloomFar * 0.8;
    color += bloom * bloomStrength;
    if (lensDirt > 0.0) {
        /* bright light scattered by the grime on the lens */
        color += bloomFar * lensDirtAt(textureCoords) * lensDirt * 1.6 * max(bloomStrength, 0.3);
    }
    if (lensWater > 0.0) {
        /* the drops' edges: dark, with the light around caught in them */
        color = color * (1.0 - 0.35 * dropRim) + bloomFar * dropRim * 0.8;
    }
    if (rainAmount > 0.0) {
        /* lit by what is around: faint in the dark, bright by the fires */
        color += rainStreaks(textureCoords) * rainAmount * (vec3(0.09, 0.085, 0.08) + bloomFar * 1.5);
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
    if (dustVeil > 0.0) {
        /* the dust: churning, billowing, rushing at us, until it is all there
         * is. Not a texture sliding across: it changes shape (3D noise moving
         * through time), turbulence folds it (domain warping), it only drifts
         * a little with the wind, and two layers expanding from the middle of
         * the picture, cross-fading, make it come at the camera */
        vec2 aspect = vec2(screenSize.x / screenSize.y, 1.0);
        vec2 fromCentre = (textureCoords - 0.5) * aspect;
        float n = 0.0, weights = 0.0;
        for (int layer = 0; layer < 2; layer++) {
            float phase = fract(time * 0.12 + float(layer) * 0.5);
            float weight = sin(phase * 3.14159265);                       /* fades in and out */
            vec2 p = fromCentre / (0.6 + phase * 1.6) + float(layer) * 7.3;   /* expanding: coming at us */
            p += vec2(time * 0.08, time * 0.02);                         /* a little drift with the wind */
            vec2 warp = vec2(dustNoise(vec3(p * 1.3, time * 0.25)), dustNoise(vec3(p * 1.3 + 5.2, time * 0.25))) - 0.5;
            p += warp * 0.9;
            float d = dustNoise(vec3(p * 2.2, time * 0.4)) * 0.6 + dustNoise(vec3(p * 5.0, time * 0.7)) * 0.4;
            n += d * weight;
            weights += weight;
        }
        n /= max(weights, 1e-3);
        vec3 dust = dustColor * (0.7 + 0.6 * n);
        float cover = clamp(dustVeil + (n - 0.5) * 3.2 * dustVeil * (1.0 - dustVeil), 0.0, 1.0);
        mapped = mix(mapped, dust, cover);
    }
    if (lightning > 0.0) {
        /* lightning in the dust storm (dust charges itself rubbing together),
         * seen through the dust: no bolt, a great soft glow somewhere up in
         * it, the dust around lit from inside, and the whole picture paling */
        vec2 aspect = vec2(screenSize.x / screenSize.y, 1.0);
        vec2 at = vec2(bolt.x, 0.55 + 0.4 * bolt.z);                       /* up in the dust */
        vec2 d = (textureCoords - at) * aspect;
        float billow = 0.75 + 0.5 * noise2(textureCoords * aspect * 4.0 + bolt.y);
        float glow = (exp(-dot(d, d) / 0.05) * 1.2 + exp(-dot(d, d) / 0.4) * 0.5) * billow;
        vec3 lit = mix(vec3(0.95, 0.93, 1.0), dustColor * 3.0 + 0.25, dustVeil);
        mapped = mix(mapped, lit, clamp(lightning * (0.3 + glow), 0.0, 1.0));
    }
    if (filmGrain > 0.0) {
        /* grain, changing every frame, stronger in the shadows */
        float n = hash12(textureCoords * screenSize + fract(time * 17.31) * 731.0) - 0.5;
        n += hash12(textureCoords * screenSize * 1.7 + fract(time * 23.7) * 517.0) - 0.5;
        mapped += n * filmGrain * (1.0 - 0.6 * luma);
    }
    outColor = vec4(clamp(mapped, 0.0, 1.0), 1.0);
}
