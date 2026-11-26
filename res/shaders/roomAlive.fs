#version 330

/* The room coming alive (Rendering/RoomAlive): reality bending round a point in the room. What is near
 * it (by where each pixel is in the room: the scene's depth) is pulled in towards it and twisted round
 * it, breathing, with a slow ripple running through walls and floor; and it goes dark where it pulls */
in vec2 textureCoords;
layout(location = 0) out vec4 outColor;

uniform sampler2D pictureSampler;
uniform sampler2D depthSampler;
uniform mat4 viewProjection, inverseViewProjection;
uniform vec3 center;        /* world */
uniform float radius;       /* how far the bending reaches (world units) */
uniform float darkRadius;   /* how far the darkness reaches */
uniform float warp;         /* 0..1 */
uniform float dark;         /* 0..1 */
uniform float time;

vec3 worldAt(vec2 uv) {
    float depth = texture(depthSampler, uv).r;
    vec4 p = inverseViewProjection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

/* 1 at the centre, 0 from radius on, smooth */
float nearness(vec3 p, float reach) {
    float n = 1.0 - smoothstep(0.0, reach, length(p - center));
    return n * n * (3.0 - 2.0 * n);
}

void main() {
    vec2 uv = textureCoords;
    vec4 c4 = viewProjection * vec4(center, 1.0);
    vec2 c = c4.w > 0.0 ? c4.xy / c4.w * 0.5 + 0.5 : vec2(0.5);
    vec3 p = worldAt(uv);
    float near = nearness(p, radius);

    /* breathing: slow, uneven, never quite the same twice */
    float breath = 0.55 + 0.3 * sin(time * 0.9) + 0.15 * sin(time * 2.3 + 1.0);
    float bend = warp * near;
    vec2 rel = uv - c;
    float angle = bend * (0.3 + 0.25 * sin(time * 0.6));               /* twisted round it */
    rel = mat2(cos(angle), -sin(angle), sin(angle), cos(angle)) * rel;
    rel *= 1.0 - bend * 0.3 * breath;                                    /* pulled in towards it */
    vec2 ripple = bend * 0.012 * vec2(sin(p.y * 0.07 + time * 2.1 + p.z * 0.03),
                                      sin(p.x * 0.06 - time * 1.7 + p.z * 0.05));
    vec2 from = clamp(c + rel + ripple, vec2(0.001), vec2(0.999));
    vec3 color = texture(pictureSampler, from).rgb;

    /* dark where it pulls: what is shown there, by how near the middle it is */
    float pulled = nearness(worldAt(from), darkRadius);
    color *= 1.0 - dark * 0.92 * pulled * (0.8 + 0.2 * breath);
    outColor = vec4(color, 1.0);
}
