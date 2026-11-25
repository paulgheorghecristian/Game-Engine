/* The Moon's destruction (the Kugelblitz), drawn into the sky: included by res/shaders/skyShader.fs
 * (#include), which calls the hooks at the end. Its values come from Rendering/MoonDestruction. */

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
uniform float formedTime;       /* seconds since the last of the moon fell in (< 0: not yet, or not seen) */
uniform float debrisShell;      /* the shell of its debris: 0 at the hole (none), 1 reached the horizon (us), 2 gone */
uniform float collapseRate;     /* collapse per second while playing (to time the gulps' outbursts) */
uniform float holeHorizon;      /* the hole's true horizon, as a fraction of its size (1: its size; BlackHole "horizon") */

/* the hole's growth as the moon pours in (as BlackHole::kugelblitzGrowth) */
float kbGrowth() {
    return smoothstep(0.62, 1.0, collapse);
}
/* near the end of its growth, the biggest pieces falling in throw matter
 * back out: this many outbursts, over the last of it (the moon mostly gone) */
const float GULPS = 7.0;
const float OUTBURSTS_FROM = 0.82;

float breakupAmount() {
    return collapse >= 0.0 ? smoothstep(0.55, 0.97, collapse) : 0.0;
}

float kbBurst(float rate, float chance, float seed);

/* the death spiral's flares: big chunks hitting the hole, and smaller
 * flickers of debris raining in between them (0 outside the breakup) */
float kbFlare() {
    float b = breakupAmount();
    float active = smoothstep(0.0, 0.08, b) * (1.0 - smoothstep(0.9, 1.0, collapse));
    return (kbBurst(2.6, 0.8, 3.1) * 2.2 + kbBurst(6.1, 0.65, 9.7) * 0.9 + kbBurst(11.0, 0.5, 5.3) * 0.4) * active;
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
/* the lopsided blast as the last of the moon falls in: part of it thrown out one way (the black hole
 * recoils the other, kicked into the orbit that brings it back: Game/recursion/STORY.md). A spray
 * of fragments and streaks in a cone round blastDirection, its front flying out across the sky,
 * cooling from white-hot to red; its dust dims the stars behind it */
uniform float blast;            /* 0: none, 1: its front far across the sky, 2: gone */
uniform vec2 blastDirection;    /* its way across the moon's disc, as seen from here (unit) */

void blastSky(vec2 q, float r, float theta, float moonRadius, inout vec3 color, inout vec3 result) {
    if (blast <= 0.0 || blast >= 2.0 || r < 0.2) return;
    float reached = min(blast, 1.0);
    float after = max(blast - 1.0, 0.0);
    /* (in these moon-radius units the edge of an angle a is at tan(a) / tan(moon); up to 70 degrees
     * out, the most the sky draws round the moon) */
    float front = max(1.5, tan(reached * 1.22) / tan(moonRadius)) * (1.0 + after * 0.5);
    float cone = smoothstep(0.5, 0.95, dot(q / r, blastDirection));
    if (cone <= 0.0 || r > front * 1.25) return;
    float u = r / front;
    /* streaks: fragments strung out behind the front, pointing away from the hole */
    vec3 rays = kbCellsWrapped(vec2(theta * 160.0, u * 0.9), 160.0);
    float streak = smoothstep(0.3, 0.0, rays.x) * step(0.1, rays.z) * smoothstep(0.02, 0.5, u) *
                   (1.0 - smoothstep(0.92, 1.12, u));
    /* clumps riding the front */
    vec3 clumps = kbCellsWrapped(vec2(theta * 90.0, u * 6.0), 90.0);
    float lump = smoothstep(0.5, 0.05, clumps.x) * step(0.15, clumps.z) * exp(-pow((u - 1.0) / 0.08, 2.0));
    float shell = exp(-pow((u - 1.0) / 0.15, 2.0));
    float t = blast * 6.0;      /* (as seconds of flight: its colour cools with them) */
    vec3 hot = mix(vec3(0.75, 0.22, 0.07), vec3(1.0, 0.62, 0.3), exp(-t * 0.15));
    hot = mix(hot, vec3(1.0, 0.92, 0.8), exp(-t * 0.9));
    float fade = 1.0 - smoothstep(0.0, 1.0, after);
    result += hot * (streak * 1.2 + lump * 2.4 + shell * 0.15) * cone * fade * 1.8;
    color *= 1.0 - 0.3 * exp(-pow((u - 0.8) / 0.4, 2.0)) * cone * fade;
}

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
        float arm = theta * 2.0 + log(r) * 0.55 + collapseTime * 0.2 + 16.0 * violence * violence;
        /* churning and shearing: the streams kink, tear and whip, harder as it grows */
        arm += violence * 0.14 * sin(r * 11.0 + collapseTime * 7.0 + theta * 12.566);
        arm += violence * 0.07 * sin(r * 27.0 - collapseTime * 13.0 + theta * 31.4);
        float band = pow(0.5 + 0.5 * cos(arm * 6.2831853), mix(5.0, 2.8, violence));
        vec3 bits = kbCellsWrapped(vec2(theta * 40.0, log(r) * 9.0 + collapseTime * 1.4 + 55.0 * violence * violence), 40.0);
        float chunk = smoothstep(mix(0.32, 0.45, violence), 0.0, bits.x) * step(mix(0.35, 0.08, violence), bits.z);
        chunk *= 1.0 + violence * 1.1 * sin(collapseTime * 26.0 + bits.z * 60.0);          /* tumbling, flashing */
        float outer = mix(1.3, 3.6, b);
        /* the streams end where they fall past the hole's edge, which grows
         * with it (in moon radii; what is behind the shadow would only be
         * wrapped round it into a blinding ring) */
        float holeGrowth = kbGrowth();
        float edge = 0.06 + holeGrowth * 1.5 * holeHorizon;
        float extent = smoothstep(edge, edge * 1.6 + 0.2, r) * (1.0 - smoothstep(outer * 0.7, outer, r));
        float hot = smoothstep(2.6, 0.25, r);
        vec3 stream = mix(vec3(0.5, 0.18, 0.08), vec3(1.0, 0.68, 0.38), hot);
        stream = mix(stream, vec3(1.0, 0.94, 0.82), hot * hot * chunk + violence * hot * 0.15);
        /* all fallen in by the end: only the black hole and its disk remain */
        float fadeOut = 1.0 - smoothstep(0.92, 1.0, collapse);
        result += stream * band * (0.3 + 2.2 * chunk) * extent * b * fadeOut * 2.2 * (1.0 + 1.1 * violence);
        /* sparks: fragments sheared off the streams as they whip round, flung
         * outwards on their own, white-hot, many as it grows */
        vec3 sparks = kbCellsWrapped(vec2(theta * 90.0 + collapseTime * 0.6, log(r) * 14.0 - collapseTime * 3.0), 90.0);
        float spark = smoothstep(0.16, 0.0, sparks.x) * step(1.0 - 0.35 * violence, sparks.z);
        result += vec3(1.0, 0.8, 0.55) * spark * extent * b * fadeOut * (0.6 + 2.5 * violence) *
                  (0.6 + 0.4 * sin(collapseTime * 30.0 + sparks.z * 50.0));

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
        float hit = mix(0.9, 0.45, b) + kbGrowth() * 1.4 * holeHorizon;
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

    /* 5b. while it grows, the biggest pieces falling in throw matter back
     * out: a flash at the hole and a spray of glowing fragments and streaks
     * to one side (where the piece fell in), bigger as the hole grows,
     * fading as it flies (the last, when the moon is gone, is the end: 6) */
    if (collapse > OUTBURSTS_FROM && collapseRate > 0.0) {
        for (int i = 1; i < int(GULPS); i++) {
            float start = OUTBURSTS_FROM + (1.0 - OUTBURSTS_FROM) * float(i) / GULPS;
            /* seconds; the collapse stops at 1 when the moon is gone, so from
             * then on the clock carries them (or they would freeze mid-flight) */
            float age = (collapse - start) / collapseRate + max(formedTime, 0.0);
            if (collapse >= 1.0 && formedTime < 0.0) continue;        /* a map loaded finished */
            if (age < 0.0 || age > 6.0) continue;
            float size = 0.4 + 0.6 * float(i) / GULPS;
            float fade = exp(-age * 0.6);
            result += vec3(1.0, 0.85, 0.65) * exp(-age * 5.0) * size * (exp(-r * r / 0.4) * 3.0 + exp(-r * 0.5) * 0.4);
            float side = fract(float(i) * 0.618034 + 0.13);                /* spread round the hole (golden ratio) */
            float toward = cos((theta - side) * 6.2831853);
            float sector = smoothstep(0.35, 0.9, toward);                 /* a spray, not a ring */
            float front = 1.2 + age * (2.5 + 2.0 * size) * (1.0 + 0.4 * size);
            if (sector > 0.0 && r < front * 1.2) {
                float u = r / front;
                vec3 rays = kbCellsWrapped(vec2(theta * 120.0 + float(i) * 13.0, u * 1.1), 120.0);
                float streak = smoothstep(0.3, 0.0, rays.x) * step(0.15, rays.z) * smoothstep(0.1, 0.7, u) *
                               (1.0 - smoothstep(0.9, 1.1, u));
                vec3 bits = kbCellsWrapped(vec2(theta * 80.0 + float(i) * 7.0, u * 5.0), 80.0);
                float lump = smoothstep(0.5, 0.05, bits.x) * step(0.2, bits.z) * exp(-pow((u - 0.95) / 0.12, 2.0));
                vec3 c = mix(vec3(0.85, 0.3, 0.1), vec3(1.0, 0.85, 0.65), exp(-age * 0.9));
                result += c * (streak * 1.1 + lump * 2.0) * sector * fade * size * 1.5;
            }
        }
    }

    /* 6. the end: the last of the moon falls in. A black hole tearing a body
     * apart throws about half of it out into space and keeps the rest: a
     * blinding burst, a shell of glowing wreckage racing outwards (radial
     * streaks behind it, cooling from white to red), and two arcs of the
     * bound half sweeping round the hole, falling back in */
    if (formedTime >= 0.0 && formedTime < 90.0) {
        float t = formedTime;
        float burst = exp(-t * 1.1);
        result += vec3(1.0, 0.92, 0.8) * burst * (exp(-r * r / 0.6) * 7.0 + exp(-r * 0.3) * 0.9);
    }
    /* the shell of debris: how far it has got is debrisShell (the timeline's, or by default
     * following the moon: RenderingMaster), not a clock, so it can be put anywhere in time */
    if (debrisShell > 0.0 && debrisShell < 2.0) {
        /* (the seconds it would have flown at the standard speed: its colour cools with them) */
        float t = debrisShell <= 1.0 ? debrisShell * 8.0 : 8.0 + (debrisShell - 1.0) * 4.0;

        /* the shell expands from the moon towards us, its edge reaching the
         * horizon (90 degrees) at debrisShell 1. It spreads across the sky at a
         * steady angle as debrisShell grows steadily (true perspective,
         * asin(radius / distance), crawls in the middle and lurches at the
         * end: it looked like it stopped, then sped up). In these gnomonic
         * moon-radius units the edge is at tan(angle) / tan(moon) */
        float reached = debrisShell;
        float edgeAngle = min(reached, 0.995) * 1.5707963;
        /* once it has passed us (1..2) it keeps flying outwards (what is left in
         * view streams away from the hole) and thins out */
        float after = max(debrisShell - 1.0, 0.0) * 4.0;
        float front = max(1.6 + reached * 16.0, tan(edgeAngle) / tan(moonRadius)) * (1.0 + after * 0.8);
        float passed = smoothstep(0.85, 1.0, reached);                          /* it is all round us now */
        float gone = 1.0 - smoothstep(0.0, 4.0, after);
        float cool = exp(-t * 0.1);
        vec3 hot = mix(vec3(0.75, 0.22, 0.07), vec3(1.0, 0.62, 0.3), cool);
        hot = mix(hot, vec3(1.0, 0.92, 0.8), exp(-t * 0.8));                   /* white-hot only at first */
        /* thrown out mostly in the plane it was spinning in: two broad fans
         * across the jets' axis, thinner towards the poles */
        vec2 across = vec2(-jetAxis.y, jetAxis.x);
        float fan = 0.25 + 0.75 * pow(abs(dot(q / max(r, 1e-3), across)), 1.5);
        if (r > 0.3 && (r < front * 1.3 || passed > 0.0)) {
            float u = r / front;
            /* the shell: clumps riding the front, ragged */
            /* (cells long along the radius: fragments flying outwards, not dots) */
            vec3 clumps = kbCellsWrapped(vec2(theta * 80.0, u * 5.0), 80.0);
            float ragged = 0.07 + 0.05 * sin(theta * 6.2831853 * 7.0 + clumps.z * 6.0);
            float shell = exp(-pow((u - 1.0) / ragged, 2.0));
            float lump = smoothstep(0.5, 0.05, clumps.x) * step(0.15, clumps.z) * (0.4 + 0.6 * clumps.z);
            /* streaks: fragments strung out behind it, pointing away from the hole */
            vec3 rays = kbCellsWrapped(vec2(theta * 140.0, u * 0.9), 140.0);
            float streak = smoothstep(0.32, 0.0, rays.x) * step(0.12, rays.z) * smoothstep(0.05, 0.6, u) *
                           (1.0 - smoothstep(0.9, 1.15, u)) * (0.5 + rays.z) * (1.0 - 0.6 * passed);
            /* and a finer, fainter layer of smaller fragments between them */
            vec3 fine = kbCellsWrapped(vec2(theta * 300.0, u * 1.6 + 3.0), 300.0);
            streak += smoothstep(0.28, 0.0, fine.x) * step(0.3, fine.z) * smoothstep(0.1, 0.7, u) *
                      (1.0 - smoothstep(0.85, 1.1, u)) * 0.5 * (1.0 - 0.6 * passed);
            float glow = (shell * (0.06 + 2.4 * lump) + streak * 1.3) * fan * mix(0.35, 1.0, cool) * smoothstep(0.0, 0.3, t);
            result += hot * glow * 1.8 * gone;
            /* the dust it carries dims the stars behind it */
            color *= 1.0 - 0.3 * shell * fan * mix(0.4, 1.0, cool) * gone;
        }
    }
    if (formedTime >= 0.0 && formedTime < 90.0) {
        float t = formedTime;
        /* the bound half: two arcs flung round the hole, lumpy, falling back */
        float bound = smoothstep(0.5, 3.0, t) * exp(-t * 0.03);
        float reach = 2.5 + 9.0 * (1.0 - exp(-t * 0.2));
        if (bound > 0.0 && r > 0.4 && r < reach * 1.2) {
            float arm = theta * 2.0 - log(r) * 1.2 + t * 0.22;                  /* orbiting, faster further in */
            arm += 0.25 * t / (0.5 + r);
            float band = pow(0.5 + 0.5 * cos(arm * 6.2831853), 4.0);
            vec3 bits = kbCellsWrapped(vec2(theta * 50.0, log(r) * 8.0 + t * 0.5), 50.0);
            float lumps = 0.5 + 2.0 * smoothstep(0.35, 0.0, bits.x) * step(0.3, bits.z);
            float span = smoothstep(0.4, 1.0, r) * (1.0 - smoothstep(reach * 0.7, reach * 1.2, r));
            vec3 c = mix(vec3(0.85, 0.32, 0.1), vec3(1.0, 0.8, 0.55), smoothstep(reach, 0.6, r));
            result += c * band * lumps * span * bound * 1.8;
        }
    }
    blastSky(q, r, theta, moonRadius, color, result);
    color += result;
}

uniform float frontEjecta;       /* pass 4: matter flying at us out of the hole (0..1) */

/* pass 4, drawn over the black hole (it is in front of it): fragments of
 * the moon blasted straight at us, out of the hole's middle, growing and
 * brightening as they come, faster and faster (log-polar cells moving
 * outwards: constant speed in log r is the looming of something coming
 * closer), a little stretched by their motion */
vec3 frontEjectaAt(vec3 dir) {
    vec3 moonDir = normalize(moonDirection);
    vec3 right = normalize(cross(moonDir, abs(moonDir.y) < 0.99 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 upAxis = cross(right, moonDir);
    float facing = dot(dir, moonDir);
    if (facing < 0.2) return vec3(0.0);
    vec2 q = vec2(dot(dir, right), dot(dir, upAxis)) / (facing * tan(moonSize));
    float r = length(q);
    float theta = atan(q.y, q.x) / 6.2831853;
    float lr = log(max(r, 0.02));
    vec3 sum = vec3(0.0);
    for (int k = 0; k < 2; k++) {
        float density = k == 0 ? 26.0 : 55.0;
        vec3 c = kbCellsWrapped(vec2(theta * density + float(k) * 7.0, lr * 1.4 - collapseTime * (0.9 + 0.5 * float(k))), density);
        /* fragments, not blobs: the cells grow with r (log-polar), the
         * fragment inside stays small */
        float size = 0.3 / (1.0 + r * 0.08);
        float blob = smoothstep(size, size * 0.2, c.x) * step(k == 0 ? 0.75 : 0.62, c.z);
        float near = smoothstep(0.4, 3.0, r) * (1.0 - smoothstep(14.0, 40.0, r));   /* out of the hole, then past us */
        vec3 hot = mix(vec3(0.95, 0.38, 0.12), vec3(1.0, 0.72, 0.45), c.z);
        sum += hot * blob * near * (0.5 + 0.5 * smoothstep(1.0, 8.0, r)) * (k == 0 ? 0.8 : 0.45);
    }
    return sum * frontEjecta;
}

/* ---- the hooks the sky shader calls ---- */

/* is the Moon being destroyed (a Kugelblitz in it)? */
bool moonDestructionOn() {
    return collapse >= 0.0;
}

/* the moon's disc (p: on the disc, its radius 1) as the crust breaks up. Death spiral: the inner
 * crust orbits faster and falls inwards, so the disc shears into a whirlpool and shrinks */
vec2 moonDestructionWarp(vec2 p) {
    float breakup = smoothstep(0.55, 0.97, collapse);
    float twist = breakup * breakup * 7.0 / (0.3 + length(p));
    return mat2(cos(twist), sin(twist), -sin(twist), cos(twist)) * p * (1.0 + breakup * 0.9);
}

/* the moon's halo as it goes */
float moonDestructionHalo() {
    return 1.0 - smoothstep(0.55, 0.8, collapse);
}
