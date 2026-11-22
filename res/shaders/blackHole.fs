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

uniform vec3 diskNormal;
uniform float diskInner, diskOuter;   /* in horizon radii */
uniform vec3 diskColor;
uniform float diskBrightness, diskOpacity, diskSpeed;
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
vec4 diskEmission(vec3 hit, vec3 rayDir, float rr) {
    vec3 radial = normalize(hit);
    vec3 orbit = normalize(cross(diskNormal, radial));
    vec3 e1 = normalize(cross(diskNormal, abs(diskNormal.y) < 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 e2 = cross(diskNormal, e1);
    /* Keplerian: inner parts orbit faster, so the turbulence shears into streaks */
    float angle = atan(dot(hit, e2), dot(hit, e1)) - time * diskSpeed * 1.6 / pow(rr, 1.5);
    vec2 q = vec2(cos(angle), sin(angle)) * rr;
    float swirl = fbm(q * 1.4 + vec2(rr * 0.35));
    float rings = 0.65 + 0.35 * sin(rr * 7.0 + swirl * 6.0);
    float t = clamp((rr - diskInner) / max(diskOuter - diskInner, 0.01), 0.0, 1.0);

    /* white-hot at the inner edge, the disk color further out, red at the rim */
    vec3 color = mix(vec3(1.0, 0.95, 0.85) * 1.5, diskColor, smoothstep(0.0, 0.4, t));
    color = mix(color, diskColor * vec3(0.85, 0.4, 0.2), smoothstep(0.45, 1.0, t));
    float intensity = diskBrightness * pow(1.0 - t, 1.3) * (0.35 + 0.9 * swirl * rings);

    /* relativistic beaming: the side orbiting towards the viewer is brighter */
    float beta = clamp(0.5 / sqrt(rr), 0.0, 0.6);
    float doppler = 1.0 / (1.0 - beta * dot(orbit, -rayDir));
    intensity *= clamp(pow(doppler, 3.0), 0.15, 6.0);

    float edges = smoothstep(diskInner, diskInner + 0.25, rr) * (1.0 - smoothstep(diskOuter * 0.7, diskOuter, rr));
    float alpha = clamp(diskOpacity * edges * (0.45 + 0.75 * swirl), 0.0, 1.0);
    return vec4(color * intensity * edges, alpha);
}

/* what an escaped ray sees: the scene where it points (or the sky) */
vec3 background(vec3 exitPosition, vec3 direction, float sceneDistance) {
    float ahead = max(sceneDistance - length(exitPosition - cameraPosition), influenceRadius);
    vec4 clip = viewProjectionMatrix * vec4(exitPosition + direction * min(ahead, 1e5), 1.0);
    if (clip.w > 0.0) {
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)))) {
            return texture(sceneSampler, uv).rgb;
        }
        if (hasSky == 0) {
            return texture(sceneSampler, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
        }
    }
    return hasSky != 0 ? texture(skySampler, direction).rgb : vec3(0.0);
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
    float h = b * b - (dot(oc, oc) - influenceRadius * influenceRadius);
    if (h < 0.0) {
        outColor = vec4(scene, 1.0);
        return;
    }
    h = sqrt(h);
    float t0 = max(-b - h, 0.0), t1 = -b + h;
    /* geometry in front of the hole stays as it is */
    if (t1 <= 0.0 || sceneDistance < max(t0, -b)) {
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
    for (int i = 0; i < 180; i++) {
        rel = pos - holePosition;
        float r = length(rel);
        if (r < rs) {
            captured = true;
            break;
        }
        if (r > influenceRadius * 1.001 && dot(rel, vel) > 0.0) {
            break;
        }
        float step = clamp(0.07 * r, rs * 0.02, rs * 1.5);
        /* light-like geodesic: photon sphere at 1.5 rs */
        vec3 acceleration = -1.5 * lensing * rs * L2 / pow(r, 5.0) * rel;
        /* the real bending is still ~2 rs / b at the edge of the sphere of
         * influence: taper it to zero over its outer half so the warped
         * region blends into the unwarped image without a visible circle */
        acceleration *= smoothstep(influenceRadius, influenceRadius * 0.45, r);
        vel = normalize(vel + acceleration * step);
        vec3 next = pos + vel * step;
        /* crossing the disk plane */
        vec3 relNext = next - holePosition;
        float s0 = dot(rel, diskNormal), s1 = dot(relNext, diskNormal);
        if (s0 * s1 < 0.0) {
            vec3 hit = mix(rel, relNext, s0 / (s0 - s1));
            float rr = length(hit) / rs;
            if (rr > diskInner && rr < diskOuter) {
                vec4 e = diskEmission(hit, vel, rr);
                light += transmittance * e.rgb * e.a;
                transmittance *= 1.0 - e.a;
            }
        }
        pos = next;
        if (transmittance < 0.01) {
            break;
        }
    }
    vec3 behind = captured ? vec3(0.0) : background(pos, vel, sceneDistance);
    outColor = vec4(light + transmittance * behind, 1.0);
}
