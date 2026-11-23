#version 330

/* Glass: drawn after the scene is composited (the deferred G-buffer has no
 * room for see-through surfaces). What is behind it is the finished image
 * there, bent a little (refraction) and tinted by the glass's colour; on top,
 * the sky reflected by Fresnel and the nearby lights' highlights. Glass hidden
 * behind something is discarded against the scene's depth. */

layout(location = 0) out vec4 outColor;

in vec3 worldPosition;
in vec3 worldNormal;

struct Material {
    vec3 diffuse;           /* the glass's tint */
    float roughness;        /* 0 clear .. frosted */
};
uniform Material material;
uniform sampler2D sceneSampler;      /* the composited image behind the glass */
uniform sampler2D depthSampler;      /* the scene's depth */
uniform samplerCube skyCube;
uniform vec2 screenSize;
uniform vec3 cameraPosition;
uniform mat4 viewMatrix;
uniform vec3 reflectionFloor;        /* a dim room reflects a little even without sky */

const int MAX_GLOW_LIGHTS = 16;
uniform int glowLightCount;
uniform vec3 glowLightPositions[MAX_GLOW_LIGHTS];
uniform vec3 glowLightColors[MAX_GLOW_LIGHTS];
uniform float glowLightRadii[MAX_GLOW_LIGHTS];

void main() {
    vec2 uv = gl_FragCoord.xy / screenSize;
    if (gl_FragCoord.z > texture(depthSampler, uv).r + 1e-6) discard;     /* behind something */

    vec3 N = normalize(worldNormal);
    vec3 V = normalize(cameraPosition - worldPosition);
    if (dot(N, V) < 0.0) N = -N;                                           /* thin glass: both sides */
    float NdotV = max(dot(N, V), 0.0);
    float F = 0.04 + 0.96 * pow(1.0 - NdotV, 5.0);

    /* what is behind, bent and tinted (frosted: blurred) */
    vec2 bend = (mat3(viewMatrix) * N).xy * 0.008;
    vec3 behind;
    if (material.roughness > 0.05) {
        vec2 px = 1.0 / screenSize;
        float r = material.roughness * 6.0;
        behind = (texture(sceneSampler, uv + bend).rgb + texture(sceneSampler, uv + bend + px * vec2(r, 0)).rgb +
                  texture(sceneSampler, uv + bend - px * vec2(r, 0)).rgb + texture(sceneSampler, uv + bend + px * vec2(0, r)).rgb +
                  texture(sceneSampler, uv + bend - px * vec2(0, r)).rgb) / 5.0;
    } else {
        behind = texture(sceneSampler, uv + bend).rgb;
    }
    vec3 color = behind * material.diffuse * (1.0 - F);

    /* reflections: the sky, and a little of the room */
    vec3 R = reflect(-V, N);
    vec3 sky = textureLod(skyCube, R, 1.0 + material.roughness * 5.0).rgb;
    color += max(sky, reflectionFloor) * F;

    /* highlights of the lights near it */
    for (int i = 0; i < glowLightCount; i++) {
        vec3 L = glowLightPositions[i] - worldPosition;
        float d = length(L);
        float att = clamp(1.0 - d / glowLightRadii[i], 0.0, 1.0);
        if (att <= 0.0) continue;
        L /= d;
        float spec = pow(max(dot(reflect(-L, N), V), 0.0), mix(400.0, 40.0, material.roughness));
        color += glowLightColors[i] * spec * att * att * (0.5 + 2.0 * F);
    }
    outColor = vec4(color, 1.0);
}
