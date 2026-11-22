#version 330

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform float pixelWidth;
uniform float pixelHeight;
uniform sampler2D textureSampler;
uniform sampler2D lensFlareColorSampler;

uniform float density;
uniform float exposure;
uniform float weight;
uniform float decay;

uniform float illuminationDecay;
uniform float uGhostDispersal;

/* 1: the flare source is the sun (World2 sun post-process): god rays are
 * blurred radially toward its screen position with fixed constants */
uniform int sunMode;
uniform vec2 sunScreenPosition;

vec4 sunFlare() {
    const float sunDensity = 1.0;
    const float sunExposure = 1.0;
    const float sunWeight = 0.025;
    const float sunDecay = 1.0;
    const int numSamples = 50;

    vec2 coords = textureCoords;
    vec2 deltaTexCoord = (textureCoords - sunScreenPosition) * (1.0 / float(numSamples)) * sunDensity;
    vec3 color = texture(textureSampler, textureCoords).xyz;
    float decayFactor = 1.0;
    for (int i = 0; i < numSamples; i++) {
        coords -= deltaTexCoord;
        color += texture(textureSampler, coords).xyz * decayFactor * sunWeight;
        decayFactor *= sunDecay;
    }
    vec4 rays = vec4(color * sunExposure, 1.0);

    vec2 texcoord = -textureCoords + vec2(1.0);
    const float ghostDispersal = 0.2;
    const int ghosts = 4;
    const float haloWidth = 0.45;
    vec2 ghostVec = (vec2(0.5) - texcoord) * ghostDispersal;
    vec4 result = vec4(0.0);
    for (int i = 0; i < ghosts; i++) {
        result += texture(textureSampler, fract(texcoord + ghostVec * float(i)));
    }
    vec2 haloVec = normalize(ghostVec) * haloWidth;
    float haloWeight = pow(1.0 - length(vec2(0.5) - fract(texcoord + haloVec)) / length(vec2(0.5)), 5.0);
    vec4 halo = texture(textureSampler, texcoord + haloVec) * haloWeight;

    vec2 lensCoord = vec2(length(vec2(0.5) - texcoord) / length(vec2(0.5)), 0.0);
    return rays + (result + halo) * texture(lensFlareColorSampler, lensCoord);
}

void main(){
    if (sunMode == 1) {
        outColor = sunFlare();
        return;
    }

    /*const float density = 2.0;
    const float exposure = 5.0;
    const float weight = 0.15;
    const float decay = 0.5;
    */
    vec2 coords = textureCoords;
    vec2 center = vec2(2.0f / pixelWidth, 2.0f / pixelHeight);
    vec2 deltaTexCoord = center;
    int numSamples = 15;
    deltaTexCoord *= (1.0f / float(numSamples)) * density;

    vec3 color = texture(textureSampler, textureCoords).xyz;
    float var_illuminationDecay = illuminationDecay;

    for(int i = 0; i < numSamples; i++){
        coords -= deltaTexCoord;
        vec3 sample = texture(textureSampler, coords).xyz;
        sample *= var_illuminationDecay * weight;
        color += sample;
        var_illuminationDecay *= decay;
    }
    vec4 outColor1 = vec4(color * exposure, 1);

    vec2 texcoord = -textureCoords + vec2(1.0);
    //float uGhostDispersal = 0.2f;
    int uGhosts = 4;
    float uHaloWidth = 0.45;

    vec2 ghostVec = (vec2(0.5) - texcoord) * uGhostDispersal;

    vec4 result = vec4(0.0);
    for (int i = 0; i < uGhosts; i++) {
        vec2 offset = fract(texcoord + ghostVec * float(i));
        result += texture(textureSampler, offset);
    }

    vec2 haloVec = normalize(ghostVec) * uHaloWidth;
    float weight2 = length(vec2(0.5) - fract(texcoord + haloVec)) / length(vec2(0.5));
    weight2 = pow(1.0 - weight2, 5.0);
    vec4 outColor2 = texture(textureSampler, texcoord + haloVec) * weight2;

    vec2 texcoord2 = vec2(length(vec2(0.5) - texcoord) / length(vec2(0.5)), 0.0);
    outColor = outColor1 + (result+outColor2) * texture(lensFlareColorSampler, texcoord2 * 1.5);
}
