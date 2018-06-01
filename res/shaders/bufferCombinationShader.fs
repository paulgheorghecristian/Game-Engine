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

uniform int outputType;

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
    return lightAcc + vec3(0.2,0.2,0.2);	//diffuse + specular + ambiental
}

vec3 blurredLightAcc() {
    return texture(blurredLightAccSampler, textureCoords).xyz * 0.75;
}

vec3 dirLightDepthMap() {
    float t2 = pow(texture(dirLightDepthSampler, textureCoords).x , 256);
    return vec3(t2, t2, t2);
}

void main() {
    if (outputType == 1) outColor = vec4(color(), 1);
	if (outputType == 2) outColor = vec4(worldNormal(), 1);
    if (outputType == 3) outColor = vec4(depth(), 1);
	if (outputType == 4) outColor = vec4(lightAccumulation(), 1);
	if (outputType == 5) {
        vec4 particlesPixel = particles();
        vec3 albedoPixel = color();

        if (depth().x != 1) {
            vec3 finalColor = albedoPixel * lightAccumulation() + blurredLightAcc();
            finalColor = particlesPixel.xyz + finalColor * (1.0 - particlesPixel.a);
            outColor = vec4(finalColor, 1);
        } else {
            outColor = vec4(particlesPixel.xyz + albedoPixel * (1.0 - particlesPixel.a), 1);
        }
    }
    if (outputType == 6) outColor = vec4(blurredLightAcc(), 1);
    if (outputType == 7) outColor = vec4(spotLightDepth(), 1);
    if (outputType == 8) outColor = vec4(dirLightDepthMap(), 1);
}
