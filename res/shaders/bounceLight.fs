#version 330

/* Light bouncing off what is near (Rendering/BounceLight): what reaches a surface from the surfaces
 * around it, lit this frame. From each pixel, rays over its hemisphere (cosine-weighted: as much as
 * each direction counts) are marched through the depth buffer for "radius" world units; where one
 * meets a surface facing back, that surface's light (the light passes' result: what it sends out,
 * and its own glow) comes in. Their mean is the light falling on the pixel from around it (a
 * Lambert surface sends on albedo times that). Rays that leave the screen or meet nothing bring
 * nothing (the map's ambient stands for the far world). The noise turns each frame: the anti-
 * aliasing's history averages it out. */

layout(location = 0) out vec4 outBounce;

in vec2 textureCoords;

uniform sampler2D depthSampler;
uniform sampler2D normalSampler;    /* eye-space normals, encoded (n + 1) / 2 */
uniform sampler2D lightSampler;     /* the light passes: with PBR already times the albedo */
uniform sampler2D albedoSampler;    /* emissive in (1 - alpha) */
uniform mat4 projectionMatrix;
uniform mat4 inverseProjectionMatrix;
uniform float radius;
uniform bool pbrEnabled;
uniform float emissiveStrength;
uniform float frame;

const int RAYS = 8;
const int STEPS = 10;

vec3 viewPosition(vec2 uv) {
    float d = textureLod(depthSampler, uv, 0.0).r;
    vec4 p = inverseProjectionMatrix * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

/* interleaved gradient noise (Jimenez): even over a few pixels, no pattern repeating */
float gradientNoise(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

/* a point (view space) on the screen: where (false: off it, or behind the eye) */
bool onScreen(vec3 S, out vec2 uv) {
    vec4 clip = projectionMatrix * vec4(S, 1.0);
    uv = clip.xy / max(clip.w, 1e-4) * 0.5 + 0.5;
    return clip.w > 0.0 && all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)));
}

/* the light a surface sends out (towards anything: taken as diffuse) */
vec3 sentFrom(vec2 uv) {
    vec3 light = textureLod(lightSampler, uv, 0.0).rgb;
    vec4 albedo = textureLod(albedoSampler, uv, 0.0);
    vec3 sent = pbrEnabled ? light : light * albedo.rgb;
    return sent + albedo.rgb * (1.0 - albedo.a) * emissiveStrength;
}

void main() {
    float depth = texture(depthSampler, textureCoords).r;
    if (depth >= 1.0) {
        outBounce = vec4(0.0);
        return;
    }
    vec3 P = viewPosition(textureCoords);
    vec3 N = normalize(texture(normalSampler, textureCoords).xyz * 2.0 - 1.0);
    /* (computed, not a tiled texture: a tile's pattern repeated drew a staircase of its size) */
    vec2 pixel = gl_FragCoord.xy + 5.588238 * frame;
    vec2 noise = vec2(gradientNoise(pixel), gradientNoise(pixel.yx * 1.37 + 17.0));
    vec3 T = normalize(cross(N, abs(N.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0)));
    vec3 B = cross(N, T);
    vec3 origin = P + N * radius * 0.01;

    vec3 sum = vec3(0.0);
    for (int i = 0; i < RAYS; i++) {
        /* cosine-weighted, the turns spread evenly round the normal */
        float phi = 6.2831853 * (float(i) + noise.x) / float(RAYS);
        float r2 = fract(noise.y + float(i) * 0.618034);
        float sinT = sqrt(r2), cosT = sqrt(1.0 - r2);
        vec3 dir = (T * cos(phi) + B * sin(phi)) * sinT + N * cosT;
        float jitter = fract(noise.x + float(i) * 0.381966);       /* (each ray its own steps) */
        float before = 0.0;
        for (int k = 0; k < STEPS; k++) {
            float t = radius * pow((float(k) + jitter) / float(STEPS), 1.6);
            vec2 uv;
            if (!onScreen(origin + dir * t, uv)) break;
            vec3 S = origin + dir * t;
            vec3 Q = viewPosition(uv);
            if (Q.z - S.z > 0.0) {
                /* it went behind what is seen there between the last step and this one: where exactly */
                float lo = before, hi = t;
                for (int r = 0; r < 5; r++) {
                    float mid = 0.5 * (lo + hi);
                    vec2 m;
                    onScreen(origin + dir * mid, m);
                    if (viewPosition(m).z - (origin + dir * mid).z > 0.0) hi = mid; else lo = mid;
                }
                S = origin + dir * hi;
                onScreen(S, uv);
                Q = viewPosition(uv);
                /* a surface it met (there, just behind it); not one far in front of where it goes */
                if (Q.z - S.z < max(radius * 0.03, (t - before) * 0.3) && length(Q - P) > radius * 0.03) {
                    vec3 Nq = normalize(textureLod(normalSampler, uv, 0.0).xyz * 2.0 - 1.0);
                    if (dot(Nq, -dir) > 0.05) sum += sentFrom(uv);
                }
                break;
            }
            before = t;
        }
    }
    outBounce = vec4(sum / float(RAYS), 1.0);
}
