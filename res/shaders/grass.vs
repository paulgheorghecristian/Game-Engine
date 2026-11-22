#version 330

/* Grass blades (GrassRenderer): one instance per cell of a world-anchored grid
 * around the camera, standing on the terrain heightfield, bent by the wind. */

layout(location = 0) in vec2 blade;   /* x across (-0.5..0.5), y up (0..1) */

uniform mat4 viewMatrix, projectionMatrix;
uniform vec3 cameraPosition;
uniform int gridSize;
uniform float spacing, radius;
uniform bool farLayer;                /* the cheap far ring (single-triangle blades) */
uniform float nearRadius;             /* far ring: where the detailed grass thins out */
uniform vec2 colorBlend;              /* distance range over which blades take the plain ground color */
uniform vec2 bladeHeight;             /* min, max */
uniform float bladeWidth;
uniform float minGroundHeight, maxSlope, clumping;
const int MAX_CLEARINGS = 8;
uniform int numClearings;
uniform vec3 clearings[MAX_CLEARINGS];   /* x, z, radius: bare ground (campfires...) */

uniform sampler2D heightSampler;      /* terrain height, top-down */
uniform vec2 fieldOrigin, fieldSize;
uniform float fieldCell;

uniform float windStrength;
uniform vec2 windDirection;           /* xz, length = the map's wind strength */
uniform float windTime;

out vec3 eyeSpaceNormal;
out vec3 worldPosition;
out float bladeT;                     /* 0 at the root, 1 at the tip */
flat out float bladeShade;
out float farBlend;                   /* 0 near .. 1 at the edge: looks like the plain ground */

vec4 hash4(vec2 p) {
    vec4 q = fract(vec4(p.xyxy) * vec4(0.1031, 0.1030, 0.0973, 0.1099));
    q += dot(q, q.wzxy + 33.33);
    return fract((q.xxyz + q.yzzw) * q.zywx);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash4(i).x, b = hash4(i + vec2(1, 0)).x, c = hash4(i + vec2(0, 1)).x, d = hash4(i + vec2(1, 1)).x;
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float groundHeight(vec2 xz) {
    return texture(heightSampler, (xz - fieldOrigin) / fieldSize).r;
}

void main() {
    ivec2 c = ivec2(gl_InstanceID % gridSize, gl_InstanceID / gridSize) - gridSize / 2;
    vec2 cell = floor(cameraPosition.xz / spacing) + vec2(c);
    vec4 r = hash4(cell);
    vec2 base = (cell + 0.1 + r.xy * 0.8) * spacing;

    vec2 uv = (base - fieldOrigin) / fieldSize;
    float h = groundHeight(base);
    float dx = groundHeight(base + vec2(fieldCell, 0.0)) - groundHeight(base - vec2(fieldCell, 0.0));
    float dz = groundHeight(base + vec2(0.0, fieldCell)) - groundHeight(base - vec2(0.0, fieldCell));
    vec3 groundNormal = normalize(vec3(-dx, 2.0 * fieldCell, -dz));

    /* where grass grows: on the terrain, above the water line, not on cliffs,
     * in natural clumps, fading out towards the edge of the radius */
    float keep = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
    keep *= step(minGroundHeight, h) * step(1.0 - maxSlope, groundNormal.y);
    float clump = valueNoise(base * 0.025) * 0.7 + valueNoise(base * 0.09) * 0.3;
    keep *= step(r.w * clumping, clump);
    for (int i = 0; i < numClearings; i++) {
        float d = length(base - clearings[i].xy) / clearings[i].z;
        keep *= step(1.0, d + r.x * 0.35);     /* ragged edge */
    }

    /* distance LOD without a visible edge: each blade has its own end
     * distance (ragged, not a circle); towards it the field thins out while
     * the remaining blades widen to keep the same coverage, and only in the
     * last stretch do they shrink away */
    float dist = length(base - cameraPosition.xz) * (0.85 + 0.3 * r.z);
    float thin = smoothstep(radius * (farLayer ? 0.45 : 0.3), radius, dist);
    keep *= step(thin * 0.8, r.y);
    if (farLayer) {
        /* fill in exactly where the detailed grass thins out */
        float nearThin = smoothstep(nearRadius * 0.3, nearRadius, dist);
        keep *= step(r.x, nearThin * 0.8 + 0.001) * step(0.001, nearThin);
    }
    float fade = 1.0 - smoothstep(radius * 0.7, radius, dist);
    float height = mix(bladeHeight.x, bladeHeight.y, r.z) * fade * keep * (0.75 + 0.5 * clump);

    float angle = r.w * 6.2831853 + r.x * 3.0;
    vec2 across = vec2(cos(angle), sin(angle));
    float t = blade.y;

    /* each blade leans its own way; the wind adds a steady push plus gust
     * waves rolling across the field */
    vec2 lean = vec2(cos(angle * 1.7 + 1.0), sin(angle * 1.7 + 1.0)) * (0.15 + 0.2 * r.y);
    vec2 dir = windDirection / max(length(windDirection), 1e-4);
    float wave = sin(windTime * 1.7 - dot(base, dir) * 0.05 + r.x * 1.5) * 0.5 + 0.5;
    float flutter = sin(windTime * 6.0 + r.y * 20.0) * 0.08;
    vec2 wind = windDirection * windStrength * (0.25 + 0.55 * wave + flutter);
    vec2 bend = (lean + wind) * t * t;
    float bendAmount = min(length(bend), 0.9);

    vec3 world = vec3(base.x, h - 0.15, base.y);
    /* far blades get wider so they stay visible instead of shrinking below a pixel */
    float widen = farLayer ? 1.0 + dist * 0.004 + thin * 1.5 : 1.0 + dist * 0.02 + thin * 2.5;
    world += vec3(across.x, 0.0, across.y) * blade.x * bladeWidth * widen * keep * (1.0 - t * 0.8);
    world += vec3(bend.x, sqrt(max(1.0 - bendAmount * bendAmount, 0.05)) * t, bend.y) * height;
    worldPosition = world;
    bladeT = t;
    bladeShade = 0.8 + 0.4 * r.z;
    farBlend = smoothstep(colorBlend.x, colorBlend.y, dist);

    /* lit mostly like the ground it stands on (soft, no harsh blade facets) */
    vec3 face = normalize(vec3(-across.y, 0.0, across.x));
    eyeSpaceNormal = mat3(viewMatrix) * normalize(mix(face, groundNormal, 0.7));
    gl_Position = projectionMatrix * viewMatrix * vec4(world, 1.0);
}
