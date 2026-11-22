#version 330

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outEyeSpaceNormal;

in vec2 texCoords;
in vec3 skyDirection;

uniform int skyMode;           /* 0: original dusk gradient, 1: night sky */
uniform vec3 zenithColor, horizonColor;
uniform float starIntensity;
/* the Milky Way: a band of light, dust lanes and dense faint stars along the
 * great circle whose pole is milkyWayNormal, brightest and widest around
 * milkyWayCenter (the galaxy's core). milkyWay: strength, 0 = none */
uniform float milkyWay;
uniform vec3 milkyWayNormal, milkyWayCenter;
uniform vec3 moonDirection, moonColor;
uniform sampler2D moonSampler; /* lunar map, equirectangular, 0 longitude in the middle */
uniform float moonPhase;       /* 0 full moon, 0.5 half (lit from the right), 1 new */
uniform float moonSize;     /* angular radius (radians) */
uniform float moonVisible;  /* 0: the map has no moon */
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

/* ---- Kugelblitz: a black hole made of pure light, ignited by accident at
 * an AI's laser facility on the moon, eating the moon from the inside.
 * collapse runs 0..1 through the stages: flash at the facility (0..0.05),
 * a glowing bore-hole and seismic rings (..0.4), cracks of molten light
 * spreading from inside (0.2..), twin plasma jets out of the poles (0.38..),
 * then the death spiral: slabs of crust shear into a whirlpool and fall in
 * until only the hole's debris ring is left (0.55..1). The hole itself is
 * drawn by BlackHole (it only shows once the crust breaks up) ---- */
uniform float collapse;         /* < 0: no Kugelblitz */
uniform vec2 collapseSite;      /* the facility, on the moon's disc (-1..1) */
uniform vec2 jetAxis;           /* the jets' direction across the disc (unit) */
uniform float collapseFlash;    /* the ignition flash, 1 at ignition fading in real time */
uniform float collapseTime;     /* seconds since Play / Restart: every playthrough is identical */

float breakupAmount() {
    return collapse >= 0.0 ? smoothstep(0.55, 0.97, collapse) : 0.0;
}

float kbBurst(float rate, float chance, float seed);

/* the death spiral's flares: big chunks hitting the hole, and smaller
 * flickers of debris raining in between them (0 outside the breakup) */
float kbFlare() {
    float b = breakupAmount();
    float active = smoothstep(0.0, 0.08, b) * (1.0 - smoothstep(0.9, 1.0, collapse));
    return (kbBurst(2.0, 0.7, 3.1) * 1.6 + kbBurst(4.3, 0.55, 9.7) * 0.6) * active;
}

/* bursts at irregular moments: `rate` chances a second, each firing with
 * probability `chance`, a sharp rise and a quick decay (on the playback
 * clock: the same every playthrough) */
float kbBurst(float rate, float chance, float seed) {
    float slot = floor(collapseTime * rate), phase = fract(collapseTime * rate);
    vec2 h = fract(sin(vec2(slot * 12.9898 + seed, slot * 78.233 + seed * 2.1)) * 43758.5453);
    return step(1.0 - chance, h.x) * (0.5 + 0.5 * h.y) * smoothstep(0.0, 0.04, phase) * exp(-phase * 5.0);
}

vec2 kbHash2(vec2 p) {
    return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}

/* cells: x = distance to the nearest point, y = distance to the cell's
 * edge (approx.), z = the cell's random id */
vec3 kbCells(vec2 x) {
    vec2 n = floor(x), f = fract(x);
    float f1 = 8.0, f2 = 8.0, id = 0.0;
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(i, j);
            vec2 o = kbHash2(n + g);
            vec2 d = g + o - f;
            float dd = dot(d, d);
            if (dd < f1) {
                f2 = f1;
                f1 = dd;
                id = kbHash2(n + g + 17.0).x;
            } else if (dd < f2) {
                f2 = dd;
            }
        }
    }
    return vec3(sqrt(f1), sqrt(f2) - sqrt(f1), id);
}

/* the same cells, wrapping every `period` along x (angles round the moon,
 * so there is no seam where the angle jumps) */
vec3 kbCellsWrapped(vec2 x, float period) {
    vec2 n = floor(x), f = fract(x);
    float f1 = 8.0, f2 = 8.0, id = 0.0;
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(i, j);
            vec2 cell = vec2(mod(n.x + g.x, period), n.y + g.y);
            vec2 o = kbHash2(cell);
            vec2 d = g + o - f;
            float dd = dot(d, d);
            if (dd < f1) {
                f2 = f1;
                f1 = dd;
                id = kbHash2(cell + 17.0).x;
            } else if (dd < f2) {
                f2 = dd;
            }
        }
    }
    return vec3(sqrt(f1), sqrt(f2) - sqrt(f1), id);
}

/* the moon's face as the hole eats it: p = point on the (sheared) disc */
void kugelblitzMoon(vec2 p, float r, inout float albedo, inout float lit, inout float disc, inout vec3 heat) {
    float ds = length(p - collapseSite);
    vec3 magma = vec3(1.0, 0.38, 0.1), whiteHot = vec3(1.0, 0.86, 0.62);

    /* the death spiral: slabs break off, the middle first (it falls into the
     * hole), then outwards; what has fallen in takes its glow with it */
    float b = breakupAmount();
    float present = 1.0, edge = 0.0;
    if (b > 0.0) {
        vec3 slabs = kbCells(p * 3.2 + 11.0);
        float order = slabs.z * 0.45 + r * 0.75;
        float threshold = b * 1.3 - 0.06;
        present = smoothstep(threshold - 0.03, threshold + 0.03, order);
        edge = 1.0 - smoothstep(0.0, 0.07, slabs.y);
    }
    vec3 surface = vec3(0.0);

    /* the bore-hole: where the singularity sank through the crust */
    float bore = smoothstep(0.02, 0.09, collapse) * (1.0 - 0.6 * smoothstep(0.5, 0.9, collapse));
    surface += mix(magma, whiteHot, exp(-ds * ds / 0.002)) * bore *
               (exp(-ds * ds / 0.003) * 3.0 + exp(-ds * ds / 0.04) * 0.5);

    /* seismic shockwaves rippling out across the surface */
    float quake = smoothstep(0.03, 0.08, collapse) * (1.0 - smoothstep(0.3, 0.45, collapse));
    float ring = pow(0.5 + 0.5 * sin(ds * 26.0 - collapseTime * 3.5), 12.0) * exp(-ds * 1.1) * quake;
    lit *= 1.0 + ring * 0.6;
    surface += vec3(0.5, 0.6, 0.8) * ring * 0.08;

    /* cracks of light from the inside, spreading from the facility (wide
     * enough to read on a moon a few dozen pixels across) */
    float spread = smoothstep(0.18, 0.55, collapse) * 2.5;
    vec3 cracks = kbCells(p * 3.6);
    float wobble = kbCells(p * 2.1 + 3.0).x * 0.35;
    float reach = 1.0 - smoothstep(spread - 0.35, spread, ds + wobble);
    float width = mix(0.05, 0.12, smoothstep(0.3, 0.85, collapse));
    float crack = (1.0 - smoothstep(0.0, width, cracks.y)) * reach;
    float pulse = 0.75 + 0.25 * sin(collapseTime * 2.3 + cracks.z * 40.0);
    /* surges of light through the cracks from inside */
    pulse += kbBurst(1.4, 0.6, 7.0) * 2.5 * smoothstep(0.28, 0.4, collapse);
    surface += mix(magma, whiteHot, crack * crack) * crack * 4.0 * pulse;
    surface += magma * exp(-cracks.y / 0.12) * reach * 0.8;   /* the glow around them */
    /* the crust chars as the inside heats up: near-black between blazing veins */
    albedo *= mix(1.0, 0.22, reach * smoothstep(0.25, 0.65, collapse));

    /* the limb glows as the inside heats up */
    surface += mix(vec3(0.6, 0.12, 0.04), magma, smoothstep(0.5, 0.9, collapse)) *
               smoothstep(0.7, 1.0, r) * smoothstep(0.3, 0.7, collapse) * 0.9;

    /* molten slab edges, and glowing rivers where slabs were */
    surface += magma * edge * b * 1.8;
    heat += surface * present * disc * (1.0 + kbFlare() * 2.5);
    heat += mix(magma, whiteHot, 0.3) * (1.0 - present) * b * (1.0 - b) * 1.2 * disc *
            (0.6 + 0.4 * sin(r * 30.0 - collapseTime * 5.0));
    disc *= present;
}

/* in the sky around the moon: the ignition flash and shock ring, gas venting
 * from the heated moon, the debris spiralling in, dust, and the polar jets.
 * Adds light to color and dims it where dust hides the stars */
void kugelblitzSky(vec3 dir, vec3 moonDir, vec3 right, vec3 upAxis, float moonRadius, inout vec3 color) {
    vec3 result = vec3(0.0);
    /* gnomonic (tangent-plane) coordinates in moon radii: straight lines on
     * the sky stay straight, and distance keeps growing (with sines it
     * saturates at 90 degrees, so the jets never reached their fade-out and
     * were cut off where this function stops) */
    vec2 q = vec2(dot(dir, right), dot(dir, upAxis)) / (max(dot(dir, moonDir), 1e-3) * tan(moonRadius));
    float r = length(q);
    float theta = atan(q.y, q.x) / 6.2831853;     /* 0..1 round the moon */

    /* 1. the flash at ignition: a blinding blue-white pinpoint at the
     * facility, a moment long, and the shock ring it sends out */
    if (collapseFlash > 0.001) {
        float f = collapseFlash;
        float d = length(q - collapseSite) * moonRadius;      /* radians */
        result += vec3(0.75, 0.85, 1.0) * f * (exp(-d * d / 1e-6) * 60.0 + exp(-d / 0.004) * 4.0 +
                                               exp(-d / 0.12) * 0.25 * f);
        float t = -log(f) / 1.4;                               /* seconds since ignition */
        float ringRadius = 1.0 + t * 2.6, ringWidth = 0.12 + 0.12 * t;
        float rough = 0.6 + 0.8 * kbCellsWrapped(vec2(theta * 30.0, t * 2.0), 30.0).x;
        result += vec3(0.6, 0.75, 1.0) * exp(-pow((r - ringRadius) / (ringWidth * rough), 2.0)) * exp(-t * 0.9) * 1.3;
    }

    /* 3. gas venting from the heated moon: ragged plumes off the limb */
    float vent = smoothstep(0.25, 0.4, collapse) * (1.0 - smoothstep(0.7, 0.9, collapse));
    if (vent > 0.0 && r > 0.9 && r < 3.0) {
        /* cells long in radius, narrow round the limb: streaks blown outwards */
        vec3 plume = kbCellsWrapped(vec2(theta * 34.0, r * 0.5 - collapseTime * 0.3), 34.0);
        float wisp = smoothstep(0.5, 0.0, plume.x) * step(0.4, plume.z);
        float flicker = 0.6 + 0.4 * sin(collapseTime * 3.0 + plume.z * 30.0);
        float fade = smoothstep(0.92, 1.05, r) * exp(-(r - 1.0) * 1.2);
        result += mix(vec3(0.5, 0.42, 0.38), vec3(1.0, 0.5, 0.2), exp(-(r - 1.0) * 2.5)) * wisp * flicker * fade * vent * 0.8;
    }

    /* 5. the death spiral: torn crust stretched into two glowing streams
     * spiralling into the hole, lumpy with chunks, white-hot as they fall in */
    float b = breakupAmount();
    if (b > 0.0 && r > 0.04 && r < 4.5) {
        /* the bigger the hole, the more violent the inflow: faster, churning,
         * thicker and hotter, more chunks tumbling in. The extra motion is
         * tied to the collapse (it speeds up smoothly as the hole grows; a
         * speed-up on the clock would jerk the streams forward) */
        float violence = smoothstep(0.62, 0.92, collapse);
        float arm = theta * 2.0 + log(r) * 0.55 + collapseTime * 0.12 + 8.0 * violence * violence;
        arm += violence * 0.06 * sin(r * 11.0 + collapseTime * 5.0 + theta * 12.566);   /* churning */
        float band = pow(0.5 + 0.5 * cos(arm * 6.2831853), mix(5.0, 3.4, violence));
        vec3 bits = kbCellsWrapped(vec2(theta * 40.0, log(r) * 9.0 + collapseTime * 0.8 + 30.0 * violence * violence), 40.0);
        float chunk = smoothstep(mix(0.32, 0.4, violence), 0.0, bits.x) * step(mix(0.35, 0.15, violence), bits.z);
        chunk *= 1.0 + violence * 0.6 * sin(collapseTime * 18.0 + bits.z * 60.0);          /* tumbling */
        float outer = mix(1.3, 3.6, b);
        /* the streams end where they fall past the hole's edge, which grows
         * with it (in moon radii; what is behind the shadow would only be
         * wrapped round it into a blinding ring) */
        float holeGrowth = smoothstep(0.62, 1.0, collapse);
        float edge = 0.06 + holeGrowth * 1.5;
        float extent = smoothstep(edge, edge * 1.6 + 0.2, r) * (1.0 - smoothstep(outer * 0.7, outer, r));
        float hot = smoothstep(2.6, 0.25, r);
        vec3 stream = mix(vec3(0.5, 0.18, 0.08), vec3(1.0, 0.68, 0.38), hot);
        stream = mix(stream, vec3(1.0, 0.94, 0.82), hot * hot * chunk + violence * hot * 0.15);
        /* all fallen in by the end: only the black hole and its disk remain */
        float fadeOut = 1.0 - smoothstep(0.92, 1.0, collapse);
        result += stream * band * (0.3 + 1.8 * chunk) * extent * b * fadeOut * 2.2 * (1.0 + 0.6 * violence);

        /* flares: big chunks hitting the hole, and smaller flickers of the
         * debris raining in between them */
        result += stream * band * extent * kbFlare() * 1.5;     /* the streams light up in flares */

        /* dust thrown up by the breakup hides the stars, lit from inside */
        float dust = extent * b * fadeOut * (0.35 + 0.5 * band);
        color *= 1.0 - dust * 0.55;
        result += vec3(0.35, 0.16, 0.08) * dust * 0.06;
    }

    /* the flares' flash, where the falling crust meets the inflow, with a
     * wide glow (outside the debris' area, or it would end in a hard edge) */
    float flare = kbFlare();
    if (flare > 0.0) {
        /* a chunk hits at one spot on the rim (its angle changes from flare
         * to flare); a flash all the way round was lensed into a ring */
        float slot = floor(collapseTime * 2.0);
        float angle = fract(sin(slot * 91.7 + 4.3) * 43758.5453) * 6.2831853;
        float hit = mix(0.9, 0.45, b) + smoothstep(0.62, 1.0, collapse) * 1.4;
        vec2 impact = vec2(cos(angle), sin(angle)) * hit;
        float d = length(q - impact);
        result += vec3(1.0, 0.86, 0.66) * flare *
                  (exp(-d * d / 0.12) * 4.0 + exp(-d * 0.6) * 0.5 + exp(-r * 0.08) * 0.05);
    }

    /* 4. twin plasma jets out of the poles, a cosmic lighthouse */
    /* they die down at the end, when there is no more moon to feed them */
    float grow = smoothstep(0.38, 0.62, collapse) * (1.0 - smoothstep(0.88, 1.0, collapse));
    if (grow > 0.0) {
        float along = dot(q, jetAxis);
        float perp = abs(dot(q, vec2(-jetAxis.y, jetAxis.x)));
        float s = abs(along);
        float len = mix(1.2, 28.0, grow);
        float width = 0.035 + 0.012 * s;
        float extent = smoothstep(0.75, 1.05, s) * (1.0 - smoothstep(len * 0.55, len, s));
        float beam = exp(-pow(perp / width, 2.0)) * extent;
        float knots = 0.65 + 0.35 * sin(s * 2.2 - collapseTime * 9.0 + (along > 0.0 ? 0.0 : 1.7));
        /* the same hot inflow as the disk: a white-hot core with an edge and
         * glow in the disk's color (blue jets clashed with the orange disk,
         * and the lens smeared their glow into blue smudges round the hole) */
        vec3 amber = blackHoleDiskColor * vec3(1.1, 0.9, 0.75);
        vec3 jetColor = mix(amber, vec3(1.0, 0.96, 0.88), exp(-pow(perp / (width * 0.4), 2.0)));
        result += jetColor * beam * knots * 2.5 * grow;
        result += amber * exp(-pow(perp / (width * 6.0), 2.0)) * extent * 0.1 * grow;
    }
    color += result;
}

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

float valueNoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash13(i), hash13(i + vec3(1, 0, 0)), f.x),
                   mix(hash13(i + vec3(0, 1, 0)), hash13(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash13(i + vec3(0, 0, 1)), hash13(i + vec3(1, 0, 1)), f.x),
                   mix(hash13(i + vec3(0, 1, 1)), hash13(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm3(vec3 p) {
    float sum = 0.0, amplitude = 0.5;
    for (int i = 0; i < 5; i++) {
        sum += valueNoise3(p) * amplitude;
        p = p * 2.03 + vec3(1.7, 9.2, 4.1);
        amplitude *= 0.5;
    }
    return sum;
}

/* the Milky Way in direction dir (added light; the stars it is made of) */
vec3 milkyWayLight(vec3 dir) {
    vec3 n = normalize(milkyWayNormal);
    float latitude = asin(clamp(dot(dir, n), -1.0, 1.0));
    /* the core: a bulge around the centre */
    vec3 c = normalize(milkyWayCenter - n * dot(milkyWayCenter, n));
    float fromCore = acos(clamp(dot(normalize(dir - n * dot(dir, n)), c), -1.0, 1.0));
    float bulge = exp(-fromCore * fromCore / 0.45);
    float width = 0.13 + 0.16 * bulge;
    /* clumpy structure along the band, and its ragged edges */
    float clumps = fbm3(dir * 6.0);
    float edge = latitude + (fbm3(dir * 4.0 + 11.0) - 0.5) * 0.12;
    float band = exp(-edge * edge / (width * width)) * (0.45 + 0.9 * clumps);
    /* dust lanes: dark filaments down the middle (the Great Rift) */
    float laneNoise = fbm3(dir * 11.0 + 3.0);
    float lanes = smoothstep(0.42, 0.62, laneNoise) * exp(-pow((latitude - 0.015) / (0.05 + 0.04 * bulge), 2.0));
    band *= 1.0 - 0.85 * lanes;
    vec3 tint = mix(vec3(0.55, 0.62, 0.85), vec3(1.0, 0.86, 0.68), bulge * 0.8 + clumps * 0.2);
    vec3 light = tint * band * (0.09 + 0.3 * bulge);
    /* countless faint stars in it */
    vec3 p = dir * 420.0;
    vec3 cell = floor(p);
    float h = hash13(cell);
    float density = band * (1.0 - lanes);
    if (h > 1.0 - 0.22 * density) {
        vec3 jitter = hash33(cell) - 0.5;
        float d = length(fract(p) - 0.5 - jitter * 0.6);
        light += mix(vec3(0.8, 0.85, 1.0), vec3(1.0, 0.9, 0.8), hash13(cell + 3.0)) * smoothstep(0.32, 0.0, d) * 0.45;
    }
    return light * milkyWay;
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
    if (milkyWay > 0.0) {
        color += milkyWayLight(dir) * starIntensity * smoothstep(0.0, 0.3, up);
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
     * for relief, faint earthshine on the dark side, a soft halo and glow.
     * With a Kugelblitz (collapse >= 0) it is being eaten from the inside */
    vec3 moonDir = normalize(moonDirection);
    float m = dot(dir, moonDir);
    float moonRadius = moonSize;
    vec3 right = normalize(cross(moonDir, abs(moonDir.y) < 0.99 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 upAxis = cross(right, moonDir);
    bool kugelblitz = collapse >= 0.0;
    float breakup = kugelblitz ? smoothstep(0.55, 0.97, collapse) : 0.0;
    if (m > cos(moonRadius * 1.1) && discs > 0.0 && moonVisible > 0.5) {
        vec2 p = vec2(dot(dir, right), dot(dir, upAxis)) / sin(moonRadius);
        /* death spiral: the inner crust orbits faster and falls inwards, so
         * the disc shears into a whirlpool and shrinks */
        float twist = breakup * breakup * 7.0 / (0.3 + length(p));
        p = mat2(cos(twist), sin(twist), -sin(twist), cos(twist)) * p * (1.0 + breakup * 0.9);
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

            vec3 heat = vec3(0.0);
            if (kugelblitz) {
                kugelblitzMoon(p, r, albedo, lit, disc, heat);
            }
            /* at night the disc hides the stars behind it; by day the bright
             * sky is in front of the moon (scattering in our atmosphere), so
             * it only adds a pale moon on top */
            color = mix(color, color * 0.4, disc * (1.0 - sunVisibility));
            color += discColor * disc * pow(albedo, 1.1) * (lit * 1.5 + earthshine);
            color += vec3(0.75, 0.8, 0.9) * disc * pow(albedo, 1.1) * lit * 0.22 * sunVisibility;
            color += heat;
        }
    }
    float halo = kugelblitz ? 1.0 - smoothstep(0.55, 0.8, collapse) : 1.0;
    color += moonColor * halo * (pow(max(m, 0.0), 2000.0) * 0.1 * discs +
                                 pow(max(m, 0.0), 300.0) * 0.05 * discs +
                                 pow(max(m, 0.0), 12.0) * 0.1);
    if (kugelblitz && m > 0.3) {
        kugelblitzSky(dir, moonDir, right, upAxis, moonRadius, color);
    }
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
