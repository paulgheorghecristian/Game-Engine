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
uniform float emissiveStrength;
uniform vec3 fogColor;
uniform float fogDensity;        /* 0: no fog */
uniform float fogHeightFalloff;
uniform float fogBaseHeight;
uniform mat4 inverseProjectionMatrix;
uniform mat4 inverseViewMatrix;
uniform float fogMoonGlow;
uniform vec3 moonDirection, moonColor;

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
    return lightAcc + vec3(ambient);	//diffuse + specular + ambiental
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
            vec3 finalColor = albedoPixel * lightAccumulation() + albedoPixel * emissive * emissiveStrength;
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
}
