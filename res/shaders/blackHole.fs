#version 330

/* Black hole (BlackHole): view rays through the sphere of influence are
 * marched along light-like Schwarzschild geodesics; the event horizon is
 * black, the accretion disk emits light, escaped rays show what lies behind
 * in the direction they leave. Drawn over a copy of the composited image. */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D sceneSampler;   /* the composited image (copy of the target) */
uniform sampler2D depthSampler;   /* scene depth */
uniform samplerCube skySampler;   /* for rays that leave the screen */
uniform int hasSky;

uniform mat4 inverseProjectionMatrix, inverseViewMatrix, viewProjectionMatrix, projectionMatrix;
uniform vec3 cameraPosition;
uniform vec3 holePosition;
uniform float holeRadius;         /* event horizon (Schwarzschild radius) */
uniform float lensing;            /* 1 = physical bending */
uniform float influenceRadius;    /* rays farther than this are not bent */
uniform float boundRadius;        /* rays are marched inside this sphere (holds the disk) */

uniform vec3 diskNormal;
uniform float diskInner, diskOuter;   /* in horizon radii */
uniform vec3 diskColor;
uniform float diskBrightness, diskOpacity, diskSpeed;
uniform float colorShift;
/* sky black holes: what they bend is the sky behind them (stars, the moon),
 * from a cloudless view centred on the hole; never the landscape or clouds,
 * which are in front */
uniform int skySource;
uniform sampler2D lensSkySampler;
uniform mat4 lensSkyViewProjection;
uniform float time;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

float fbm(vec2 p) {
    float n = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        n += a * noise(p);
        p = mat2(1.6, -1.2, 1.2, 1.6) * p + 3.7;
        a *= 0.5;
    }
    return n;
}

/* light from the disk where the ray crosses it at `hit` (relative to the
 * hole); rr = distance in horizon radii; rgb = radiance, a = opacity */
/* footprint: disk size (in horizon radii) one pixel covers there. Detail
 * finer than a pixel is faded out, or a distant disk sparkles with dots */
vec4 diskEmission(vec3 hit, vec3 rayDir, float rr, float footprint) {
    vec3 radial = normalize(hit);
    vec3 orbit = normalize(cross(diskNormal, radial));
    vec3 e1 = normalize(cross(diskNormal, abs(diskNormal.y) < 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 e2 = cross(diskNormal, e1);
    /* Keplerian: inner parts orbit faster, so the turbulence shears into
     * streaks. The shear would grow without bound (hair-thin threads after a
     * few minutes), so two layers each advect for one period and restart,
     * crossfaded: the shear stays bounded while the gas keeps flowing */
    const float PERIOD = 5.0;
    float base = atan(dot(hit, e2), dot(hit, e1));
    float omega = diskSpeed * 1.6 / pow(rr, 1.5);
    float phase0 = fract(time / PERIOD), phase1 = fract(time / PERIOD + 0.5);
    float a0 = base - omega * PERIOD * (phase0 - 0.5);
    float a1 = base - omega * PERIOD * (phase1 - 0.5);
    float s0 = fbm(vec2(cos(a0), sin(a0)) * rr * 1.4 + vec2(rr * 0.35) + floor(time / PERIOD) * 7.31);
    float s1 = fbm(vec2(cos(a1), sin(a1)) * rr * 1.4 + vec2(rr * 0.35) + floor(time / PERIOD + 0.5) * 7.31 + 3.7);
    float w = abs(1.0 - 2.0 * phase0);   /* layer 0 fades out as it restarts */
    float swirl = mix(s0, s1, w);
    /* fine streaks along the orbit: noise that changes fast with radius and
     * slowly around the disk (the long thin filaments of a sheared disk) */
    float t0 = fbm(vec2(rr * 3.2, 1.7) + vec2(cos(a0), sin(a0)) * 0.9 + floor(time / PERIOD) * 5.13);
    float t1 = fbm(vec2(rr * 3.2, 1.7) + vec2(cos(a1), sin(a1)) * 0.9 + floor(time / PERIOD + 0.5) * 5.13 + 2.9);
    float streaks = clamp(0.5 + (mix(t0, t1, w) - 0.5) * 1.6, 0.0, 1.0);
    streaks = mix(streaks, 0.5, smoothstep(0.06, 0.25, footprint));
    /* crossfading two noises flattens contrast; restore it */
    swirl = clamp(0.5 + (swirl - 0.5) * 1.35, 0.0, 1.0);
    swirl = mix(swirl, 0.5, smoothstep(0.25, 0.9, footprint));
    /* faint banding only: the turbulence carries the structure */
    float rings = 0.9 + 0.1 * sin(rr * 5.0 + swirl * 6.0);
    /* thin-disk temperature, T ~ r^-3/4 (1 - sqrt(r_in / r))^1/4, scaled to 1
     * where it peaks (just outside the inner edge, at 49/36 r_in): the
     * innermost lip is cooler, the disk cools outwards */
    float peak = diskInner * 49.0 / 36.0;
    float temperature = pow(rr / peak, -0.75) *
                        pow(max(1.0 - sqrt(diskInner / rr), 0.0) / (1.0 - 6.0 / 7.0), 0.25);

    /* shift of the light on its way to the viewer: Doppler (gas orbiting
     * towards the viewer is blueshifted, away redshifted, plus time dilation)
     * times gravitational redshift (climbing out of the well dims and reddens
     * the inner disk) */
    float beta = clamp(0.5 / sqrt(rr), 0.0, 0.6);
    float doppler = sqrt(1.0 - beta * beta) / (1.0 - beta * dot(orbit, -rayDir));
    float g = doppler * sqrt(max(1.0 - 1.5 / rr, 0.05));

    /* observed color from the shifted temperature: red rim, the disk color,
     * white-hot, and blue-white where it is blueshifted beyond that */
    float seen = temperature * mix(1.0, g, colorShift);
    vec3 color = mix(diskColor * vec3(0.8, 0.32, 0.12), diskColor, smoothstep(0.45, 0.72, seen));
    color = mix(color, vec3(1.0, 0.93, 0.84) * 1.3, smoothstep(0.88, 1.04, seen));
    color = mix(color, vec3(0.72, 0.84, 1.3) * 1.4, smoothstep(1.08, 1.4, seen));

    /* brightness: the hot parts radiate most; beaming (g^3) makes the
     * approaching side brighter (applied in full whatever colorShift is) */
    float intensity = diskBrightness * 1.5 * pow(temperature, 2.2) * (0.5 + 0.7 * swirl * rings) *
                      mix(0.65, 1.3, streaks);
    intensity *= clamp(pow(g, 3.0), 0.1, 6.0);

    float edges = smoothstep(diskInner, diskInner + 0.25, rr) * (1.0 - smoothstep(diskOuter * 0.55, diskOuter, rr));
    float alpha = clamp(diskOpacity * edges * (0.45 + 0.75 * swirl) * mix(0.55, 1.0, streaks), 0.0, 1.0);
    return vec4(color * intensity * edges, alpha);
}

/* what an escaped ray sees: the scene where it points (or the sky) */
vec3 background(vec3 exitPosition, vec3 direction, float sceneDistance) {
    float ahead = max(sceneDistance - length(exitPosition - cameraPosition), boundRadius);
    /* the sky is at infinity: only the direction matters */
    vec4 clip = sceneDistance > 1e8 ? viewProjectionMatrix * vec4(direction, 0.0)
                                    : viewProjectionMatrix * vec4(exitPosition + direction * min(ahead, 1e5), 1.0);
    vec2 uv = clip.xy / max(clip.w, 1e-6) * 0.5 + 0.5;
    /* near the Einstein ring the lens squeezes a wide band of the image into
     * a thin ring, so neighbouring pixels sample far apart: filter by how far
     * (mipmapped copy), or the squeezed detail aliases into ripples */
    vec2 duvx = dFdx(uv), duvy = dFdy(uv);
    vec3 ddx = dFdx(direction), ddy = dFdy(direction);
    /* softer still where it is squeezed hard: the concentric copies of cloud
     * edges are real lensing, but crisp they read as ripples. Unsqueezed
     * sky keeps its full resolution */
    float px = 1.0 / float(textureSize(sceneSampler, 0).y);
    float squeeze = max(length(duvx), length(duvy)) / px;
    float soften = mix(1.0, 3.0, smoothstep(1.5, 6.0, squeeze));
    duvx *= soften; duvy *= soften; ddx *= soften; ddy *= soften;
    if (clip.w > 0.0) {
        if (all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)))) {
            return textureGrad(sceneSampler, uv, duvx, duvy).rgb;
        }
        if (hasSky == 0) {
            return texture(sceneSampler, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
        }
    }
    return hasSky != 0 ? textureGrad(skySampler, direction, ddx, ddy).rgb : vec3(0.0);
}

/* light-like geodesic: photon sphere at 1.5 rs. The real bending is still
 * ~2 rs / b at the edge of the sphere of influence: taper it to zero over its
 * outer half so the warped region blends into the unwarped image without a
 * visible circle */
vec3 bend(vec3 rel, float L2, float rs) {
    float r = length(rel);
    return -1.5 * lensing * rs * L2 / pow(r, 5.0) * rel * smoothstep(influenceRadius, influenceRadius * 0.45, r);
}

/* ---- world black holes: the bent rays are traced against the depth
 * buffer (like screen-space reflections), so geometry around and behind the
 * hole is seen where the bent ray really meets it. A ray that is barely bent
 * meets the surface at (almost) its own pixel: no seams ---- */
float sceneViewDepth(vec2 uv) {
    float d = texture(depthSampler, uv).r;
    if (d >= 0.99999) return 1e9;
    vec4 v = inverseProjectionMatrix * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return -v.z / v.w;
}

/* mip level for what a traced ray sees: rays that came close to the hole
 * see a band of the world squeezed into a thin ring; read that from a
 * blurrier mip, or it breaks into jagged sub-pixel dashes */
float traceLod = 0.0;

/* did the ray cross the visible surface on its way to p? It is behind the
 * surface now, and the surface lies beyond where the ray was at the previous
 * step (`previousDepth`, view depth). A surface well in front of that is a
 * foreground object the ray passes behind, not one it hits. depth: the
 * ray's view depth at p; uv.x < 0 when p is off screen */
bool crossesScene(vec3 p, float previousDepth, float tolerance, out vec2 uv, out float depth) {
    vec4 clip = viewProjectionMatrix * vec4(p, 1.0);
    depth = clip.w;
    uv = vec2(-1.0);
    if (clip.w <= 0.0) return false;
    vec2 screen = clip.xy / clip.w * 0.5 + 0.5;
    if (any(lessThan(screen, vec2(0.0))) || any(greaterThan(screen, vec2(1.0)))) return false;
    uv = screen;
    float sceneZ = sceneViewDepth(uv);
    return clip.w > sceneZ && sceneZ > previousDepth - tolerance;
}

/* is p behind the visible surface (and where on screen)? */
bool behindScene(vec3 p, out vec2 uv) {
    vec4 clip = viewProjectionMatrix * vec4(p, 1.0);
    uv = clip.xy / max(clip.w, 1e-6) * 0.5 + 0.5;
    return clip.w > 0.0 && clip.w > sceneViewDepth(clamp(uv, vec2(0.0), vec2(1.0)));
}

/* the hit lies between a (in front) and b (behind): bisect to where the ray
 * crosses the surface, so silhouettes seen through the lens are not
 * stair-stepped by the march's step length */
vec2 refineHit(vec3 a, vec3 b, vec2 uv) {
    for (int k = 0; k < 6; k++) {
        vec3 m = (a + b) * 0.5;
        vec2 mUV;
        if (behindScene(m, mUV)) {
            b = m;
            uv = mUV;
        } else {
            a = m;
        }
    }
    return uv;
}

/* a ray leaving the sphere at p along d: march on (growing steps) until it
 * meets the scene; if it never does, it sees the sky that way */
vec3 traceOut(vec3 p, vec3 d) {
    float t = 0.0, stepLength = boundRadius * 0.05;
    float previousDepth = (viewProjectionMatrix * vec4(p, 1.0)).w;
    vec2 uv;
    float depth;
    for (int i = 0; i < 40; i++) {
        t += stepLength;
        if (crossesScene(p + d * t, previousDepth, stepLength * 0.25, uv, depth)) {
            uv = refineHit(p + d * (t - stepLength), p + d * t, uv);
            return textureLod(sceneSampler, uv, traceLod).rgb;
        }
        if (uv.x < 0.0) break;   /* off screen or behind the camera */
        previousDepth = depth;
        stepLength *= 1.15;
    }
    /* the sky in that direction: the screen where it shows sky there */
    vec4 clip = viewProjectionMatrix * vec4(d, 0.0);
    if (clip.w > 0.0) {
        vec2 skyUV = clip.xy / clip.w * 0.5 + 0.5;
        if (all(greaterThanEqual(skyUV, vec2(0.0))) && all(lessThanEqual(skyUV, vec2(1.0))) &&
            texture(depthSampler, skyUV).r >= 0.99999) {
            return textureLod(sceneSampler, skyUV, traceLod).rgb;
        }
    }
    if (hasSky == 0) return vec3(0.0);
    if (d.y < 0.0) {
        /* off screen and below the horizon: distant ground we have no image
         * of. The cube only holds a flat ground color there (a green-grey
         * band round the hole); far land fades into the horizon haze, so use
         * that, darker the steeper down the ray looks */
        vec3 horizon = textureLod(skySampler, normalize(vec3(d.x, 0.02, d.z)), traceLod * 0.5 + 1.0).rgb;
        return horizon * mix(1.0, 0.55, smoothstep(0.0, -0.6, d.y));
    }
    return textureLod(skySampler, d, traceLod * 0.5).rgb;
}

/* the sky behind the hole in direction d (the cube far off the hole) */
vec3 lensSky(vec3 d) {
    vec4 clip = lensSkyViewProjection * vec4(d, 0.0);
    vec2 uv = clip.xy / max(clip.w, 1e-6) * 0.5 + 0.5;
    vec2 duvx = dFdx(uv), duvy = dFdy(uv);
    vec3 ddx = dFdx(d), ddy = dFdy(d);
    float px = 1.0 / float(textureSize(lensSkySampler, 0).y);
    float soften = mix(1.0, 3.0, smoothstep(1.5, 6.0, max(length(duvx), length(duvy)) / px));
    if (clip.w > 0.0 && all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)))) {
        return textureGrad(lensSkySampler, uv, duvx * soften, duvy * soften).rgb;
    }
    return hasSky != 0 ? textureGrad(skySampler, d, ddx * soften, ddy * soften).rgb : vec3(0.0);
}

/* a sky pixel looking along `view`, bent to `bent`: the sky behind swapped
 * for the bent one, and what our atmosphere adds in front of it (haze,
 * thin cloud, particles, the sunset glow) kept as it was, unbent. Only
 * close to the shadow, where the bending is strong and the disk's light
 * covers the change, is it the bent sky alone (switching there instead of
 * far out avoids a visible circle where the clean lens sky meets the hazy
 * real one) */
vec3 skyBehind(vec3 view, vec3 bent, vec3 scene) {
    vec3 lensed = lensSky(bent);
    vec3 here = lensSky(view);
    float k = smoothstep(0.08, 0.35, length(bent - view));
    return mix(max(scene + lensed - here, vec3(0.0)), lensed, k);
}

void main() {
    vec3 scene = texture(sceneSampler, textureCoords).rgb;

    vec4 farPoint = inverseProjectionMatrix * vec4(textureCoords * 2.0 - 1.0, 1.0, 1.0);
    vec3 dir = normalize(mat3(inverseViewMatrix) * (farPoint.xyz / farPoint.w));
    float depth = texture(depthSampler, textureCoords).r;
    vec4 viewPoint = inverseProjectionMatrix * vec4(textureCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    float sceneDistance = depth >= 0.99999 ? 1e9 : length(viewPoint.xyz / viewPoint.w);

    /* the sphere of influence along the straight ray */
    vec3 oc = cameraPosition - holePosition;
    float b = dot(oc, dir);
    float h = b * b - (dot(oc, oc) - boundRadius * boundRadius);
    if (h < 0.0) {
        outColor = vec4(scene, 1.0);
        return;
    }
    h = sqrt(h);
    float t0 = max(-b - h, 0.0), t1 = -b + h;
    /* geometry in front of the hole stays as it is: for a sky hole
     * everything is in front; for a world hole what is before the sphere
     * (geometry inside it is met by the traced rays) */
    bool worldHole = skySource == 0;
    if (t1 <= 0.0 || sceneDistance < (worldHole ? t0 : max(t0, -b))) {
        outColor = vec4(scene, 1.0);
        return;
    }

    float rs = holeRadius;
    vec3 pos = cameraPosition + dir * t0;
    vec3 vel = dir;
    vec3 rel = pos - holePosition;
    vec3 L = cross(rel, vel);
    float L2 = dot(L, L);
    vec3 light = vec3(0.0);
    float transmittance = 1.0;
    bool captured = false;
    bool hitGeometry = false;
    vec3 hitColor = vec3(0.0);
    float closest = 1e9;
    int crossings = 0;       /* times the ray caught the disk */
    int innerCrossings = 0;  /* times it went through the disk's plane inside its inner edge */
    float captureEdge = max(1.5 * lensing, 1.0) * rs;
    float previousDepth = (viewProjectionMatrix * vec4(pos, 1.0)).w;
    /* leapfrog (kick-drift-kick, second order): plain Euler with a step that
     * grows with distance makes the total bending jump from pixel to pixel,
     * which shows as faint concentric ripples in the lensed sky */
    vec3 acceleration = bend(rel, L2, rs);
    for (int i = 0; i < 180; i++) {
        rel = pos - holePosition;
        float r = length(rel);
        closest = min(closest, r);
        if (r < rs) {
            captured = true;
            break;
        }
        if (r > boundRadius * 1.001 && dot(rel, vel) > 0.0) {
            break;
        }
        /* longer steps outside the bending (straight there; the disk
         * crossing is found exactly whatever the step) */
        float step = clamp(0.07 * r, rs * 0.02, r > influenceRadius ? rs * 2.5 : rs * 0.8);
        vel += acceleration * (step * 0.5);
        vec3 next = pos + normalize(vel) * step;
        acceleration = bend(next - holePosition, L2, rs);
        vel = normalize(vel + acceleration * (step * 0.5));
        /* crossing the disk plane */
        vec3 relNext = next - holePosition;
        float s0 = dot(rel, diskNormal), s1 = dot(relNext, diskNormal);
        if (s0 * s1 < 0.0) {
            vec3 hit = mix(rel, relNext, s0 / (s0 - s1));
            float rr = length(hit) / rs;
            if (rr > diskInner && rr < diskOuter) {
                /* one pixel's size there, in horizon radii */
                float footprint = length(holePosition + hit - cameraPosition) * 2.0 /
                                  (projectionMatrix[1][1] * float(textureSize(sceneSampler, 0).y) * rs);
                /* rays grazing the capture edge (the photon ring) see the
                 * disk squeezed into a hairline: no texture there, or the
                 * ring breaks into dots */
                footprint += smoothstep(captureEdge * 1.7, captureEdge * 1.1, closest) * 2.0;
                vec4 e = diskEmission(hit, vel, rr, footprint);
                /* higher-order images (light caught on a second pass through
                 * the disk, after looping round the hole) are hairlines far
                 * thinner than a pixel: only a few pixels' rays catch them,
                 * as scattered dots. Keep just a trace of them */
                /* a ray that already went through the disk's inner hole (right
                 * by the black hole) and looped round makes a secondary image
                 * of the disk: a hairline, scattered dots. (The arcs of the
                 * far side bent over and under the hole cross the plane only
                 * far out, outside the disk) */
                float order = innerCrossings == 0 && (!worldHole || crossings == 0) ? 1.0 : 0.08;
                /* a ray already turned back by more than ~100 degrees sees
                 * the disk squeezed into a hairline (scattered dots) */
                if (worldHole) order *= smoothstep(-0.55, -0.1, dot(vel, dir));
                e.rgb *= order;
                e.a *= order;
                crossings++;
                light += transmittance * e.rgb * e.a;
                transmittance *= 1.0 - e.a;
            }
            if (rr <= diskInner) innerCrossings++;
        }
        pos = next;
        if (transmittance < 0.01) {
            break;
        }
        vec2 hitUV;
        float depthHere;
        if (worldHole && crossesScene(pos, previousDepth, step * 0.25 + rs * 0.1, hitUV, depthHere)) {
            hitGeometry = true;
            hitColor = textureLod(sceneSampler, refineHit(pos - normalize(vel) * step, pos, hitUV),
                                  clamp((max(1.5 * lensing, 1.0) * rs * 2.4 - closest) / (rs * 0.35), 0.0, 4.0)).rgb;
            break;
        }
        previousDepth = depthHere;
    }
    /* rays grazing the capture edge (photon sphere at 1.5 * lensing rs, or the
     * horizon) swing round in arbitrary directions: what they show is the
     * world behind the viewer squeezed into a ring a pixel or two thick,
     * and off screen we only have a flat stand-in for it (a hard grey-green
     * rim). Real images show it dark, apart from the disk's light, which is
     * kept: fade what is behind over a thin band outside the shadow */
    float rim = smoothstep(captureEdge * 1.02, captureEdge * 1.45, closest);
    /* a world hole's rays turned past ~90 degrees head back behind the
     * viewer, where the screen has nothing to show (they fell back to a flat
     * ground color: a mottled grey-green band round the shadow); dark there */
    if (worldHole && !captured) {
        rim *= smoothstep(-0.35, 0.15, dot(vel, dir));
    }
    traceLod = clamp((captureEdge * 2.4 - closest) / (rs * 0.35), 0.0, 4.0);
    /* the photon ring: escaping rays that grazed the capture edge catch the
     * disk in a window far thinner than a pixel (a dotted ring) */
    if (!captured) {
        /* (narrower for the sky hole: its arcs pass just outside this band) */
        light *= worldHole ? smoothstep(captureEdge * 1.03, captureEdge * 1.35, closest)
                           : smoothstep(captureEdge * 1.01, captureEdge * 1.12, closest);
    }
    vec3 behind = captured || rim <= 0.0 ? vec3(0.0)
                : hitGeometry ? hitColor
                : worldHole ? traceOut(pos, vel)
                : sceneDistance > 1e8 ? skyBehind(dir, vel, scene)
                : background(pos, vel, sceneDistance);
    /* a world hole's rays that came close see the world squeezed into a
     * sliver: hit-or-miss per pixel turns outlines there into stripes. Blend
     * into a blurred average of the surroundings instead */
    if (worldHole && !captured && hasSky != 0) {
        float squeezed = smoothstep(captureEdge * 4.2, captureEdge * 1.8, closest);
        if (squeezed > 0.0) {
            vec3 around = vel.y >= 0.0 ? textureLod(skySampler, vel, 3.0).rgb
                        : textureLod(skySampler, normalize(vec3(vel.x, 0.02, vel.z)), 3.0).rgb *
                          mix(1.0, 0.55, smoothstep(0.0, -0.6, vel.y));
            behind = mix(behind, around, squeezed);
        }
    }
    outColor = vec4(light + transmittance * behind * rim, 1.0);
}
