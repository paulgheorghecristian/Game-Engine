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
            vec3 finalColor = albedoPixel * lightAccumulation();
            finalColor = particlesPixel.xyz + finalColor * (1.0 - particlesPixel.a);
            finalColor = volumetricPixel.xyz + finalColor * (1.0 - volumetricPixel.a);
            finalColor = flarePixel.xyz * flarePixel.a + finalColor;
            outColor = finalColor;
        } else {
            vec3 finalColor = particlesPixel.xyz + albedoPixel * (1.0 - particlesPixel.a);
            finalColor = volumetricPixel.xyz + finalColor * (1.0 - volumetricPixel.a);
            finalColor = flarePixel.xyz * flarePixel.a + finalColor;
            outColor = finalColor;
        }

        //float gamma = 1.0f / 2.2f;
        //outColor.rgb = pow(outColor.rgb, vec3(gamma));
    }
    if (outputType == 7) outColor = spotLightDepth();
    if (outputType == 8) outColor = dirLightDepthMap();
    //if (outputType == 9) outColor = volumetrics();
    if (outputType == 10) outColor = roughnessMap();
}
