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
uniform float moonSize;     /* angular radius (radians) */
uniform float time;
/* a black hole in the sky tearing a stream of dust off the moon */
uniform float moonStream;            /* 0: none */
uniform vec3 blackHoleDirection, blackHoleDiskNormal, blackHoleDiskColor;
uniform float blackHoleDiskAngle;    /* outer edge of its disk, radians on the sky */

float streamHash(float x) { return fract(sin(x * 127.1) * 43758.5453); }
float streamNoise(float x) {
    float i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(streamHash(i), streamHash(i + 1.0), f);
}

/* tangent-plane coordinates (radians) of a direction around the hole */
vec2 holePlane(vec3 d, vec3 e1, vec3 e2, vec3 h) {
    return vec2(dot(d, e1), dot(d, e2)) / max(dot(d, h), 1e-3);
}

/* moon dust torn off the moon, curving around the hole into its disk:
 * grey moonlit dust near the moon, heated to the disk's color as it falls in */
vec3 moonDustStream(vec3 dir, vec3 moonDir) {
    vec3 h = normalize(blackHoleDirection);
    if (dot(dir, h) < 0.5 || dot(moonDir, h) < 0.5) return vec3(0.0);
    vec3 e1 = normalize(cross(h, abs(h.y) < 0.99 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 e2 = cross(e1, h);
    vec2 p = holePlane(dir, e1, e2, h);
    vec2 moonP = holePlane(moonDir, e1, e2, h);
    /* where it joins the disk: the disk point nearest the moon's side */
    vec3 n = normalize(blackHoleDiskNormal);
    vec3 towardsMoon = moonDir - h * dot(moonDir, h);
    vec3 inDisk = normalize(towardsMoon - n * dot(towardsMoon, n));
    vec3 endDir = normalize(h + inDisk * tan(blackHoleDiskAngle * 0.5));
    vec2 endP = holePlane(endDir, e1, e2, h);
    /* bent around the hole in the sense of the orbit */
    vec2 mid = (moonP + endP) * 0.5;
    vec2 side = normalize(vec2(-(endP - moonP).y, (endP - moonP).x));
    if (dot(side, mid) < 0.0) side = -side;
    vec2 control = mid + side * length(endP - moonP) * 0.45;

    float best = 1e9, bestT = 0.0;
    for (int i = 0; i <= 28; i++) {
        float t = float(i) / 28.0;
        vec2 q = mix(mix(moonP, control, t), mix(control, endP, t), t);
        float d = length(p - q);
        if (d < best) { best = d; bestT = t; }
    }
    float width = mix(moonSize * 0.6, 0.003, bestT);
    float core = exp(-pow(best / width, 2.0));
    float clumps = 0.55 + 0.45 * streamNoise(bestT * 38.0 - time * 1.2);
    float ends = smoothstep(0.0, 0.08, bestT) * (1.0 - smoothstep(0.88, 1.0, bestT));
    vec3 dust = mix(vec3(0.62, 0.64, 0.7), blackHoleDiskColor * 2.2, smoothstep(0.25, 1.0, bestT));
    return dust * core * clumps * ends * moonStream * 0.55;
}
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

    if (moonStream > 0.0) {
        color += moonDustStream(dir, normalize(moonDirection)) * mix(0.35, 1.0, 1.0 - sunVisibility);
    }

    /* moon: a sphere lit by the sun (so it has a terminator and phases), with
     * the albedo map for maria and craters and the same map as a height field
     * for relief, faint earthshine on the dark side, a soft halo and glow */
    vec3 moonDir = normalize(moonDirection);
    float m = dot(dir, moonDir);
    float moonRadius = moonSize;
    if (m > cos(moonRadius * 1.1) && discs > 0.0) {
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
            /* lit a little from below (at night the sun is under the horizon),
             * which also tilts the terminator the way a real night moon shows */
            vec3 side = normalize(right * 0.94 - upAxis * 0.34);
            vec3 sunOnMoon = normalize(-moonDir * cos(phaseAngle) + side * sin(phaseAngle));
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

uniform vec2 cloudCutCos;        /* cosines of the inner / outer edge of the hole's cloud cut */
uniform float holeCloudClear;    /* 0..1: how much the clouds thin out around a sky black hole */

/* the cloud layer in direction dir: its color and opacity */
void cloudLayer(vec3 dir, out vec3 cloudColor, out float alpha) {
    cloudColor = vec3(0.0);
    alpha = 0.0;
    if (cloudCoverage <= 0.0 || dir.y <= 0.0) return;
    float t = (CLOUD_HEIGHT - cameraPosition.y) / max(dir.y, 0.02);
    vec2 xz = cameraPosition.xz + dir.xz * t;
    float d = cloudDensity(xz);
    if (d <= 0.001) return;

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
    cloudColor = mix(nightColor, dayColor, sunVisibility);

    /* thinner and hazier towards the horizon */
    float fade = smoothstep(0.0, 0.14, dir.y) * exp(-t * 0.000035);
    alpha = d * fade * 0.95;
    /* thinner around a black hole in the sky, so it is never buried */
    if (holeCloudClear > 0.0) {
        float near = smoothstep(cloudCutCos.y - 0.1, cloudCutCos.x, dot(dir, normalize(blackHoleDirection)));
        alpha *= 1.0 - holeCloudClear * near;
    }
}

/* A black hole in the sky lenses what is behind it (stars, the moon), but
 * the clouds are in front of it. So the main view draws the sky without the
 * clouds around the hole (pass 1), the hole bends that, and then those clouds
 * are blended back on top (pass 2), weighted so the two passes add up to the
 * plain sky. Reflections use the plain sky (pass 0). Pass 3 is the sky
 * behind the hole with no clouds at all, which the hole bends. */
uniform int cloudPass;
uniform vec3 cloudCutDirection;
uniform sampler2D sceneDepth;    /* pass 2: only over the sky */
uniform vec2 screenSize;

void main(){
    if (skyMode == 1) {
        vec3 dir = normalize(skyDirection);
        if (cloudPass == 3) {
            outColor = vec4(nightSky(dir), 1.0);
            outEyeSpaceNormal = vec3(0);
            return;
        }
        float cut = cloudPass == 0 ? 0.0 : smoothstep(cloudCutCos.y, cloudCutCos.x, dot(dir, cloudCutDirection));
        if (cloudPass == 2) {
            if (cut <= 0.0 || texture(sceneDepth, gl_FragCoord.xy / screenSize).r < 1.0) discard;
            vec3 cloudColor;
            float alpha;
            cloudLayer(dir, cloudColor, alpha);
            /* over a sky that already has alpha * (1 - cut) of the cloud */
            float rest = alpha * cut / max(1.0 - alpha + alpha * cut, 1e-4);
            outColor = vec4(cloudColor, rest);
            outEyeSpaceNormal = vec3(0);
            return;
        }
        vec3 cloudColor;
        float alpha;
        cloudLayer(dir, cloudColor, alpha);
        outColor = vec4(mix(nightSky(dir), cloudColor, alpha * (1.0 - cut)), 1.0);
    } else {
        outColor = vec4(mix(vec3(0.22, 0.25, 0.58), vec3(0.6, 0.20, 0.1), texCoords.y-0.2), 1) * 0.3;
    }
    outEyeSpaceNormal = vec3(0);
}
