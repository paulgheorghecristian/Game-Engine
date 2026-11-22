#version 330

/* Screen-space ambient occlusion: how much of the hemisphere above each
 * pixel is blocked by nearby geometry (view space, 16 samples, rotated per
 * pixel with blue noise). Output: 1 = open, 0 = fully occluded. */

layout(location = 0) out vec4 outAO;

in vec2 textureCoords;

uniform sampler2D depthSampler;
uniform sampler2D normalSampler;   /* eye-space normals, encoded (n + 1) / 2 */
uniform sampler2D noiseSampler;
uniform mat4 projectionMatrix;
uniform mat4 inverseProjectionMatrix;
uniform vec2 noiseScale;            /* screen size / noise texture size */
uniform float radius;               /* world units */

const int KERNEL_SIZE = 16;
const vec3 kernel[KERNEL_SIZE] = vec3[](
    vec3(-0.0521, 0.0088, 0.0368),
    vec3(0.0246, -0.0849, 0.0300),
    vec3(-0.0055, 0.0627, 0.0444),
    vec3(0.0012, 0.0121, 0.0300),
    vec3(0.0165, 0.0058, 0.0300),
    vec3(-0.0478, 0.1371, 0.1013),
    vec3(0.0371, -0.0582, 0.0742),
    vec3(-0.1095, 0.0625, 0.2145),
    vec3(0.0768, 0.2375, 0.1172),
    vec3(0.1071, -0.3132, 0.0462),
    vec3(-0.0822, -0.3208, 0.1350),
    vec3(0.0501, -0.2351, 0.0495),
    vec3(0.1631, -0.5076, 0.0313),
    vec3(-0.1987, -0.0644, 0.4229),
    vec3(0.2166, 0.1342, 0.7021),
    vec3(0.0054, -0.0525, 0.2746)
);

vec3 viewPosition(vec2 uv) {
    float d = texture(depthSampler, uv).r;
    vec4 p = inverseProjectionMatrix * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

void main() {
    float depth = texture(depthSampler, textureCoords).r;
    if (depth >= 1.0) {
        outAO = vec4(1.0);
        return;
    }
    vec3 P = viewPosition(textureCoords);
    vec3 N = normalize(texture(normalSampler, textureCoords).xyz * 2.0 - 1.0);
    vec3 random = vec3(texture(noiseSampler, textureCoords * noiseScale).xy * 2.0 - 1.0, 0.0);
    vec3 T = normalize(random - N * dot(random, N));
    mat3 TBN = mat3(T, cross(N, T), N);

    float occlusion = 0.0;
    for (int i = 0; i < KERNEL_SIZE; i++) {
        vec3 S = P + TBN * kernel[i] * radius;
        vec4 offset = projectionMatrix * vec4(S, 1.0);
        vec2 uv = offset.xy / offset.w * 0.5 + 0.5;
        float sceneZ = viewPosition(uv).z;
        /* only count occluders within the radius (no halos from far geometry) */
        float range = smoothstep(0.0, 1.0, radius / max(abs(P.z - sceneZ), 1e-4));
        occlusion += (sceneZ >= S.z + 0.02 * radius ? 1.0 : 0.0) * range;
    }
    outAO = vec4(vec3(1.0 - occlusion / float(KERNEL_SIZE)), 1.0);
}
