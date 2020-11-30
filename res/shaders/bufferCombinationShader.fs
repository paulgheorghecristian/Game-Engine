#version 330

in vec2 textureCoords;
out vec4 outColor;

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
    return lightAcc + vec3(0.2);	//diffuse + specular + ambiental
}

vec3 blurredLightAcc() {
#if 1
    return texture(blurredLightAccSampler, textureCoords).xyz;
#endif
    return vec3(0);
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
    if (outputType == 1) outColor = vec4(color(), 1);
	if (outputType == 2) outColor = vec4(worldNormal(), 1);
    if (outputType == 3) outColor = vec4(depth(), 1);
	if (outputType == 4) outColor = vec4(lightAccumulation(), 1);
	if (outputType == 5) {
        vec4 particlesPixel = particles();
        vec4 volumetricPixel = volumetrics();
        vec3 albedoPixel = color();
        vec4 flarePixel = flareMap();// + flareBlurMap();

        vec3 depthVal = depth();

        if (depthVal.x != 1) {
            vec3 finalColor = albedoPixel * lightAccumulation();
            finalColor = particlesPixel.xyz + finalColor * (1.0 - particlesPixel.a);
            finalColor = volumetricPixel.xyz + finalColor * (1.0 - volumetricPixel.a);
            finalColor = flarePixel.xyz * flarePixel.a + finalColor;
            outColor = vec4(finalColor, 1);
        } else {
            vec3 finalColor = particlesPixel.xyz + albedoPixel * (1.0 - particlesPixel.a);
            finalColor = volumetricPixel.xyz + finalColor * (1.0 - volumetricPixel.a);
            finalColor = flarePixel.xyz * flarePixel.a + finalColor;
            outColor = vec4(finalColor, 1);
        }
    }
    if (outputType == 6) outColor = vec4(blurredLightAcc(), 1);
    if (outputType == 7) outColor = vec4(spotLightDepth(), 1);
    if (outputType == 8) outColor = vec4(dirLightDepthMap(), 1);
    if (outputType == 9) outColor = volumetrics();
    if (outputType == 10) outColor = vec4(roughnessMap(), 1);
}
