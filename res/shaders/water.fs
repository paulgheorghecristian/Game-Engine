#version 330

/* Planar water (ported from World2's waterShader.fs): DuDv-distorted
 * reflection + refraction, normal map, plus Fresnel, depth murk, soft
 * shorelines, light glints and fog for the deferred pipeline. */

layout(location = 0) out vec4 outColor;

in vec3 worldPosition;
in vec2 waveCoords;

uniform sampler2D reflectionSampler;
uniform sampler2D refractionSampler;
uniform sampler2D dudvSampler;
uniform sampler2D normalMapSampler;
uniform sampler2D depthSampler;
uniform sampler2D particlesSampler;
uniform sampler2D volumetricLightSampler;
uniform sampler2D flareSampler;

uniform mat4 projectionMatrix;
uniform vec2 screenSize;
uniform vec3 cameraPosition;
uniform float time;
uniform int hasReflection;

uniform float waveStrength, waveSpeed;
uniform vec3 shallowColor, deepColor;
uniform float murkDepth, softEdge, reflectivity;

uniform vec3 moonDirection, moonColor;
uniform vec3 fogColor;
uniform float fogDensity, fogHeightFalloff, fogBaseHeight, fogMoonGlow;

const int MAX_LIGHTS = 8;
uniform int numLights;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform vec4 lightSpots[MAX_LIGHTS];
uniform float lightRadii[MAX_LIGHTS];

float linearDepth(float d) {
    return projectionMatrix[3][2] / ((2.0 * d - 1.0) + projectionMatrix[2][2]);
}

float fogAmount(float dist, float height) {
    if (fogDensity <= 0.0) return 0.0;
    float heightFactor = exp(-max(height - fogBaseHeight, 0.0) * fogHeightFalloff);
    return clamp((1.0 - exp(-dist * fogDensity)) * heightFactor, 0.0, 1.0);
}

void main() {
    vec2 uv = gl_FragCoord.xy / screenSize;
    float sceneDepth = texture(depthSampler, uv).r;
    if (gl_FragCoord.z > sceneDepth) {
        discard; /* terrain / objects in front of the water */
    }
    float surfaceZ = linearDepth(gl_FragCoord.z);
    float waterDepth = linearDepth(sceneDepth) - surfaceZ;

    /* World2 DuDv scheme, plus a second, slower layer for less tiling */
    float offset = fract(time * waveSpeed);
    vec2 distorted = texture(dudvSampler, vec2(waveCoords.x + offset, waveCoords.y)).rg * 0.1;
    distorted = waveCoords + vec2(distorted.x, distorted.y + offset);
    vec2 distortion = (texture(dudvSampler, distorted).rg * 2.0 - 1.0) * waveStrength *
                      clamp(waterDepth / 20.0, 0.0, 1.0);

    vec3 n1 = texture(normalMapSampler, distorted).rgb;
    vec3 n2 = texture(normalMapSampler, waveCoords * 0.37 - vec2(offset * 0.6, offset * 0.3)).rgb;
    vec3 normal = normalize(vec3(n1.r * 2.0 - 1.0, n1.b * 2.5, n1.g * 2.0 - 1.0) +
                            vec3(n2.r * 2.0 - 1.0, n2.b * 2.5, n2.g * 2.0 - 1.0));

    vec3 toCamera = cameraPosition - worldPosition;
    float distanceToCamera = length(toCamera);
    vec3 V = toCamera / distanceToCamera;

    /* refraction: don't pull in geometry that is in front of the water */
    vec2 refractionUV = clamp(uv + distortion, vec2(0.001), vec2(0.999));
    float refractionDepth = texture(depthSampler, refractionUV).r;
    if (refractionDepth < gl_FragCoord.z) {
        refractionUV = uv;
        refractionDepth = sceneDepth;
    }
    vec3 refraction = texture(refractionSampler, refractionUV).rgb * shallowColor;
    float murk = clamp((linearDepth(refractionDepth) - surfaceZ) / murkDepth, 0.0, 1.0);
    refraction = mix(refraction, deepColor, murk);

    vec3 reflection = deepColor;
    if (hasReflection != 0) {
        reflection = texture(reflectionSampler, clamp(uv + distortion, vec2(0.001), vec2(0.999))).rgb;
    }

    float cosTheta = max(dot(V, normalize(mix(vec3(0, 1, 0), normal, 0.25))), 0.0);
    float fresnel = clamp((0.02 + 0.98 * pow(1.0 - cosTheta, 5.0)) * reflectivity, 0.0, 1.0);
    vec3 color = mix(refraction, reflection, fresnel);

    /* glints: the moon and every nearby light */
    vec3 specular = moonColor * pow(max(dot(reflect(-moonDirection, normal), V), 0.0), 400.0) * 2.0;
    for (int i = 0; i < numLights; i++) {
        vec3 L = lightPositions[i] - worldPosition;
        float d = length(L);
        float att = clamp(1.0 - d / lightRadii[i], 0.0, 1.0);
        if (att <= 0.0) continue;
        L /= d;
        if (lightSpots[i].w >= -1.0 && dot(-L, lightSpots[i].xyz) < lightSpots[i].w) continue;
        att *= att;
        vec3 H = normalize(L + V);
        specular += lightColors[i] * att * (pow(max(dot(normal, H), 0.0), 160.0) * 2.5 + 0.06);
    }
    color += specular * (1.0 - murk * 0.3);

    /* soft shoreline */
    float edge = clamp(waterDepth / softEdge, 0.0, 1.0);
    color = mix(texture(refractionSampler, uv).rgb, color, edge);

    vec3 hazeColor = fogColor + moonColor * fogMoonGlow * pow(max(dot(-V, moonDirection), 0.0), 6.0);
    color = mix(color, hazeColor, fogAmount(distanceToCamera, worldPosition.y));

    /* re-apply what the combination pass composited over the scene */
    vec4 particles = texture(particlesSampler, uv);
    vec4 volumetric = texture(volumetricLightSampler, uv);
    vec4 flare = texture(flareSampler, uv);
    color = particles.rgb + color * (1.0 - particles.a);
    color = volumetric.rgb + color * (1.0 - volumetric.a);
    color = flare.rgb * flare.a + color;

    outColor = vec4(color, 1.0);
}
