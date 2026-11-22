#version 330

in vec2 textureCoords;

layout(location = 0) out vec3 outColor;

uniform sampler2D normalSampler;
uniform sampler2D depthSampler;
uniform sampler2D lightAccumulationSampler;
uniform sampler2D colorSampler;
uniform sampler2D blurredLightAccSampler;
uniform sampler2D spotLightDepthMap;
uniform sampler2D particlesSampler;
uniform sampler2D dirLightDepthSampler;
uniform sampler2D volumetricLightSampler;
uniform sampler2D flareSampler;
uniform sampler2D flareBlurSampler;
uniform sampler2D roughnessSampler;

uniform int outputType;

uniform float ambient;           /* engine default 0.1 */
uniform vec3 ambientSky;         /* hemisphere ambient: up-facing ... */
uniform vec3 ambientGround;      /* ... and down-facing surfaces */
uniform sampler2D aoSampler;     /* screen-space ambient occlusion (1 = open) */
uniform float aoStrength;        /* 0 = off */
uniform bool hdrEnabled;         /* false: light is clamped at 1.0 like the old 8-bit buffer */
uniform bool pbrEnabled;         /* light buffer already includes albedo; roughness target = (roughness, metallic) */
uniform bool skyReflections;     /* reflect the sky cube (blurrier for rougher surfaces) */
uniform samplerCube skyCube;
uniform float skyCubeMaxLod;


uniform float emissiveStrength;
uniform vec3 fogColor;
uniform float fogDensity;        /* 0: no fog */
uniform float fogHeightFalloff;
uniform float fogBaseHeight;
uniform mat4 inverseProjectionMatrix;
uniform mat4 inverseViewMatrix;
uniform float fogMoonGlow;
uniform vec3 moonDirection, moonColor;

/* PBR: lit color = direct light (from the light passes) + ambient light,
 * diffuse for dielectrics plus a Fresnel-weighted specular share from the sky */
vec3 pbrComposite(vec3 albedo) {
    vec3 direct = texture(lightAccumulationSampler, textureCoords).xyz;
    if (!hdrEnabled) {
        direct = min(direct, vec3(1.0));
    }
    vec3 surface = texture(roughnessSampler, textureCoords).rgb;
    float roughness = clamp(surface.r, 0.05, 1.0);
    float metallic = clamp(surface.g, 0.0, 1.0);

    vec3 nEye = texture(normalSampler, textureCoords).xyz * 2.0 - 1.0;
    float d = texture(depthSampler, textureCoords).x;
    vec4 view = inverseProjectionMatrix * vec4(textureCoords * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec3 V = normalize(-view.xyz / view.w);
    vec3 N = length(nEye) > 0.1 ? normalize(nEye) : V;
    float NdotV = max(dot(N, V), 0.0);
    vec3 worldN = normalize(mat3(inverseViewMatrix) * N);

    float occlusion = aoStrength > 0.0 ? mix(1.0, texture(aoSampler, textureCoords).r, aoStrength) : 1.0;
    vec3 hemisphere = mix(ambientGround, ambientSky, worldN.y * 0.5 + 0.5);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);
    vec3 diffuseAmbient = (1.0 - F) * (1.0 - metallic) * albedo * hemisphere;
    vec3 specularAmbient;
    if (skyReflections) {
        vec3 worldV = normalize(mat3(inverseViewMatrix) * V);
        vec3 R = reflect(-worldV, worldN);
        /* reflections pointing into the surface would be the ground: fade them */
        float horizon = clamp(1.0 + dot(R, worldN), 0.0, 1.0);
        vec3 sky = textureLod(skyCube, R, roughness * (skyCubeMaxLod - 1.0)).rgb;
        specularAmbient = F * sky * horizon * horizon;
    } else {
        specularAmbient = F * ambientSky * (1.0 - 0.6 * roughness);
    }
    return direct + (diffuseAmbient + specularAmbient) * occlusion;
}

/* fog factor for the pixel at textureCoords with depth buffer value d;
 * also returns the fog color, brightened toward the moon */
float fogAmount(float d, out vec3 color) {
    color = fogColor;
    if (fogDensity <= 0.0) return 0.0;
    vec4 view = inverseProjectionMatrix * vec4(textureCoords * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    view /= view.w;
    vec3 world = (inverseViewMatrix * view).xyz;
    vec3 rayDir = normalize(mat3(inverseViewMatrix) * view.xyz);
    color += moonColor * fogMoonGlow * pow(max(dot(rayDir, moonDirection), 0.0), 6.0);
    float heightFactor = exp(-max(world.y - fogBaseHeight, 0.0) * fogHeightFalloff);
    return clamp((1.0 - exp(-length(view.xyz) * fogDensity)) * heightFactor, 0.0, 1.0);
}

vec4 volumetrics() {
    return texture(volumetricLightSampler, textureCoords);
}

vec4 particles () {
    return texture (particlesSampler, textureCoords);
}

vec3 depth() {
    float t2 = pow(texture(depthSampler, textureCoords).x , 256);
    return vec3(t2, t2, t2);
}
vec3 spotLightDepth() {
    float t2 = pow(texture(spotLightDepthMap, textureCoords).x , 256);
    return vec3(t2, t2, t2);
}
vec3 color() {
    vec3 albedoColor = texture(colorSampler, textureCoords).xyz;

    return albedoColor;
}
vec3 worldNormal() {
    return texture(normalSampler, textureCoords).xyz;
}
vec3 lightAccumulation() {
    vec3 lightAcc = texture (lightAccumulationSampler, textureCoords).xyz;
    if (!hdrEnabled) {
        lightAcc = min(lightAcc, vec3(1.0));
    }
    vec3 n = texture(normalSampler, textureCoords).xyz * 2.0 - 1.0;
    float up = length(n) > 0.1 ? normalize(mat3(inverseViewMatrix) * n).y : 1.0;
    float occlusion = aoStrength > 0.0 ? mix(1.0, texture(aoSampler, textureCoords).r, aoStrength) : 1.0;
    return lightAcc + mix(ambientGround, ambientSky, up * 0.5 + 0.5) * occlusion;	//diffuse + specular + ambiental
}

vec3 dirLightDepthMap() {
    float t2 = pow(texture(dirLightDepthSampler, textureCoords).x , 256);
    return vec3(t2, t2, t2);
}
vec4 flareMap() {
    return texture(flareSampler, textureCoords);
}
vec4 flareBlurMap() {
    return texture(flareBlurSampler, textureCoords);
}
vec3 roughnessMap() {
    return texture(roughnessSampler, textureCoords).xyz;
}

void main() {
    if (outputType == 1) outColor = color();
	if (outputType == 2) outColor = worldNormal();
    if (outputType == 3) outColor = depth();
	if (outputType == 4) outColor = lightAccumulation();
	if (outputType == 5) {
        vec4 particlesPixel = particles();
        vec4 volumetricPixel = volumetrics();
        vec3 albedoPixel = color();
        vec4 flarePixel = flareMap();// + flareBlurMap();

        vec3 depthVal = depth();

        vec3 lightAcc = lightAccumulation();
        float exposure = 0.5f;
        // exposure tone mapping
        vec3 mapped = vec3(1.0) - exp(-lightAcc * exposure);

        if (depthVal.x != 1) {
            vec4 albedoEmissive = texture(colorSampler, textureCoords);
            float emissive = 1.0 - albedoEmissive.a;
            vec3 finalColor = (pbrEnabled ? pbrComposite(albedoPixel) : albedoPixel * lightAccumulation()) +
                              albedoPixel * emissive * emissiveStrength;
            vec3 hazeColor;
            float haze = fogAmount(texture(depthSampler, textureCoords).x, hazeColor);
            finalColor = mix(finalColor, hazeColor, haze);
            finalColor = particlesPixel.xyz + finalColor * (1.0 - particlesPixel.a);
            finalColor = volumetricPixel.xyz + finalColor * (1.0 - volumetricPixel.a);
            finalColor = flarePixel.xyz * flarePixel.a + finalColor;
            outColor = finalColor;// * max(0.3, (1.0 - depthVal.x));
        } else {
            vec3 finalColor = particlesPixel.xyz + albedoPixel * (1.0 - particlesPixel.a);
            finalColor = volumetricPixel.xyz + finalColor * (1.0 - volumetricPixel.a);
            finalColor = flarePixel.xyz * flarePixel.a + finalColor;
            outColor = finalColor;
        }

        //float gamma = 1.0f / 2.2f;
        //outColor.rgb = pow(outColor.rgb, vec3(gamma));
    }
    if (outputType == 6) outColor = lightAccumulation();
    if (outputType == 7) outColor = spotLightDepth();
    if (outputType == 8) outColor = dirLightDepthMap();
    //if (outputType == 9) outColor = volumetrics();
    if (outputType == 10) outColor = roughnessMap();
    if (outputType == 9) outColor = vec3(texture(aoSampler, textureCoords).r);
}
