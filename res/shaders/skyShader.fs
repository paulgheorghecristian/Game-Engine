#version 330

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outEyeSpaceNormal;

in vec2 texCoords;
in vec3 skyDirection;

uniform int skyMode;           /* 0: original dusk gradient, 1: night sky */
uniform vec3 zenithColor, horizonColor;
uniform float starIntensity;
uniform vec3 moonDirection, moonColor;
uniform sampler2D moonSampler; /* lunar map, equirectangular, 0 longitude in the middle */
uniform float moonPhase;       /* 0 full moon, 0.5 half (lit from the right), 1 new */
uniform vec3 cameraPosition;
/* ---- clouds: one drifting fbm layer at CLOUD_HEIGHT (world units); the
 * sky draws it and the directional light pass uses the same density for the
 * cloud shadows on the ground, so the two always match ---- */
uniform float cloudCoverage;   /* 0: no clouds .. 1: overcast */
uniform vec2 cloudWind;        /* drift in world units per second */
uniform float cloudTime;       /* seconds */
const float CLOUD_HEIGHT = 1500.0;

float cloudHash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float cloudNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(cloudHash(i), cloudHash(i + vec2(1, 0)), u.x),
               mix(cloudHash(i + vec2(0, 1)), cloudHash(i + vec2(1, 1)), u.x), u.y);
}

/* 0..1 cloud density above world position xz */
float cloudDensity(vec2 xz) {
    vec2 p = (xz + cloudWind * cloudTime) * 0.0009;
    float n = 0.0, amp = 0.5;
    mat2 rot = mat2(0.8, -0.6, 0.6, 0.8);
    for (int i = 0; i < 5; i++) {
        n += amp * cloudNoise(p);
        p = rot * p * 2.07 + vec2(7.3, 1.9);
        amp *= 0.5;
    }
    float threshold = mix(0.68, 0.3, cloudCoverage);
    return smoothstep(threshold, threshold + 0.22, n);
}
 /* full-moon albedo (maria, craters), on the disc */
uniform float time;
uniform vec3 sunDirection, sunColor, sunsetColor;
uniform float sunVisibility;   /* 0 at night */
uniform float sunsetStrength;  /* horizon glow around sunrise / sunset */
uniform bool reflectionPass;   /* rendering the reflection cube: no sun / moon discs
                                * (their highlights come from the direct light) */

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

vec3 nightSky(vec3 dir) {
    float up = clamp(dir.y, 0.0, 1.0);
    vec3 color = mix(horizonColor, zenithColor, pow(up, 0.45));
    /* the air near the horizon is thicker: a paler band blending into the haze */
    color = mix(color, horizonColor * 1.12, pow(1.0 - up, 7.0) * 0.6);

    /* stars: one candidate per cell of a 3D grid around the sky sphere */
    vec3 p = dir * 140.0;
    vec3 cell = floor(p);
    float h = hash13(cell);
    if (h > 0.972) {
        vec3 jitter = hash33(cell) - 0.5;
        float d = length(fract(p) - 0.5 - jitter * 0.6);
        float twinkle = 0.65 + 0.35 * sin(time * (1.5 + h * 4.0) + h * 91.0);
        float brightness = (h - 0.972) / 0.028;
        brightness = brightness * brightness * 2.2;      /* a few standout bright stars */
        vec3 tint = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.9, 0.75), hash13(cell + 7.0));
        color += tint * smoothstep(0.2 + 0.08 * brightness, 0.0, d) * twinkle * (0.55 + brightness) * 1.6 *
                 starIntensity * smoothstep(0.02, 0.25, up);
    }

    /* sunrise / sunset glow on the horizon around the sun */
    vec2 flatDir = dir.xz / max(length(dir.xz), 1e-4);
    vec2 flatSun = sunDirection.xz / max(length(sunDirection.xz), 1e-4);
    float towardSun = max(dot(flatDir, flatSun), 0.0);
    color += sunsetColor * sunsetStrength * (0.25 + pow(towardSun, 4.0)) * pow(1.0 - up, 3.0);

    /* sun disc and glow */
    float s = dot(dir, normalize(sunDirection));
    float discs = reflectionPass ? 0.0 : 1.0;
    color += sunColor * sunVisibility * (smoothstep(0.9993, 0.9996, s) * 4.0 * discs +
                                         pow(max(s, 0.0), 500.0) * 1.2 * discs +
                                         pow(max(s, 0.0), 24.0) * 0.18 +
                                         pow(max(s, 0.0), 4.0) * 0.12); /* broad scattering glow */

    /* moon: a sphere lit by the sun (so it has a terminator and phases), with
     * the albedo map for maria and craters and the same map as a height field
     * for relief, faint earthshine on the dark side, a soft halo and glow */
    vec3 moonDir = normalize(moonDirection);
    float m = dot(dir, moonDir);
    const float moonRadius = 0.026;                       /* angular radius (radians) */
    if (m > 0.995 && discs > 0.0) {
        vec3 right = normalize(cross(moonDir, abs(moonDir.y) < 0.99 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
        vec3 upAxis = cross(right, moonDir);
        vec2 p = vec2(dot(dir, right), dot(dir, upAxis)) / sin(moonRadius);
        float r = length(p);
        if (r < 1.02) {
            float z = sqrt(max(1.0 - r * r, 0.0));
            /* the visible face of the globe: latitude / longitude of this point
             * of the sphere (the near side faces us) */
            float longitude = atan(p.x, z);
            float latitude = asin(clamp(p.y, -1.0, 1.0));
            vec2 uv = vec2(0.5 + longitude / 6.2831853, 0.5 + latitude / 3.14159265);
            float albedo = texture(moonSampler, uv).r;
            /* a little relief from the map (crater rims are bright) */
            float e = 1.0 / 1024.0;
            float hx = texture(moonSampler, uv + vec2(e, 0)).r - texture(moonSampler, uv - vec2(e, 0)).r;
            float hy = texture(moonSampler, uv + vec2(0, e)).r - texture(moonSampler, uv - vec2(0, e)).r;

            /* sphere normal (towards the viewer is -moonDir), bumped */
            vec3 n = normalize(right * p.x + upAxis * p.y - moonDir * z);
            n = normalize(n - (right * hx + upAxis * hy) * 1.5);
            /* sunlight on the moon for its phase: from behind the viewer at
             * full moon, from the side at half moon */
            float phaseAngle = moonPhase * 3.14159265;
            vec3 sunOnMoon = normalize(-moonDir * cos(phaseAngle) + right * sin(phaseAngle));
            float lit = max(dot(n, sunOnMoon), 0.0);
            /* the full moon is nearly flat-lit (little limb darkening), with a
             * soft terminator */
            lit = smoothstep(0.0, 0.2, lit) * pow(lit, 0.45);
            float earthshine = 0.035;
            float disc = 1.0 - smoothstep(0.955, 1.0, r);
            vec3 discColor = mix(moonColor, vec3(dot(moonColor, vec3(0.3333))), 0.6);
            /* at night the disc hides the stars behind it; by day the bright
             * sky is in front of the moon (scattering in our atmosphere), so
             * it only adds a pale moon on top */
            color = mix(color, color * 0.4, disc * (1.0 - sunVisibility));
            color += discColor * disc * pow(albedo, 1.1) * (lit * 1.5 + earthshine);
            color += vec3(0.75, 0.8, 0.9) * disc * pow(albedo, 1.1) * lit * 0.22 * sunVisibility;
        }
    }
    color += moonColor * (pow(max(m, 0.0), 2000.0) * 0.1 * discs +
                          pow(max(m, 0.0), 300.0) * 0.05 * discs +
                          pow(max(m, 0.0), 12.0) * 0.1);
    return color;
}

vec3 clouds(vec3 dir, vec3 sky) {
    if (cloudCoverage <= 0.0 || dir.y <= 0.0) return sky;
    float t = (CLOUD_HEIGHT - cameraPosition.y) / max(dir.y, 0.02);
    vec2 xz = cameraPosition.xz + dir.xz * t;
    float d = cloudDensity(xz);
    if (d <= 0.001) return sky;

    /* light from the sun by day, the moon by night: sample the density a bit
     * towards the light; denser there = this part is in the cloud's shadow */
    vec3 L = sunVisibility > 0.0 ? normalize(sunDirection) : normalize(moonDirection);
    float towardsLight = cloudDensity(xz + L.xz / max(L.y, 0.15) * 160.0);
    float lit = clamp(1.0 - towardsLight * 0.75 + (1.0 - d) * 0.25, 0.0, 1.0);

    vec3 shade = mix(horizonColor, zenithColor, 0.35) * 0.9 + vec3(0.08);
    vec3 dayColor = mix(shade, sunColor * 1.15, lit);
    dayColor += sunsetColor * sunsetStrength * (0.35 + 0.65 * lit);
    /* silver lining: thin edges glow when looking towards the sun */
    float toSun = max(dot(dir, normalize(sunDirection)), 0.0);
    dayColor += sunColor * pow(toSun, 12.0) * (1.0 - d) * 1.5;
    vec3 nightColor = horizonColor * 0.6 + moonColor * 0.12 * lit;
    vec3 cloudColor = mix(nightColor, dayColor, sunVisibility);

    /* thinner and hazier towards the horizon */
    float fade = smoothstep(0.0, 0.14, dir.y) * exp(-t * 0.000035);
    return mix(sky, cloudColor, d * fade * 0.95);
}

void main(){
    if (skyMode == 1) {
        vec3 dir = normalize(skyDirection);
        outColor = vec4(clouds(dir, nightSky(dir)), 1.0);
    } else {
        outColor = vec4(mix(vec3(0.22, 0.25, 0.58), vec3(0.6, 0.20, 0.1), texCoords.y-0.2), 1) * 0.3;
    }
    outEyeSpaceNormal = vec3(0);
}
