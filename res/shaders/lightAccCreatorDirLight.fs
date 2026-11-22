#version 330

layout(location = 0) out vec3 outLight;

uniform vec3 lightColor, lightDirection;
uniform int screenWidth, screenHeight;
uniform sampler2D dirLightDepthSampler;
uniform sampler2D eyeSpaceNormalSampler;
uniform sampler2D depthSampler;
uniform sampler2D roughnessSampler;
uniform sampler2D blueNoiseSampler;
uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 inverseViewMatrix; /* inverse(viewMatrix), computed on the CPU */
uniform vec3 cameraForwardVector;
uniform vec3 cameraForwardVectorEyeSpace;

uniform mat4 dirLightProjMatrix, dirLightViewMatrix;

in vec3 viewRay;

const int PCFStrength = 1;
const int PCFKernelSideSize = PCFStrength * 2 + 1;
const int PCFStartingIndex = PCFKernelSideSize / 2;
const int PCFKernelSize = PCFKernelSideSize * PCFKernelSideSize;
const float depthMapTexelSize = 1.0f/2048.0f; /* TODO remove hardcode */

/* TODO remove hardcode */
const float maxDist = 500.0f;

flat in vec3 lightDirectionEyeSpace;

uniform int castsShadow; /* 0: skip the shadow-map test */


/* ---- PBR (Cook-Torrance: GGX distribution, Smith-Schlick visibility,
 * Schlick Fresnel; metal/roughness workflow) ---- */
uniform bool pbrEnabled;
uniform sampler2D albedoSampler;   /* G-buffer albedo (PBR: the light pass applies it) */

vec3 pbrShade(vec2 uv, vec3 P, vec3 N, vec3 L, vec3 radiance) {
    const float PI = 3.14159265;
    vec3 albedo = texture(albedoSampler, uv).rgb;
    vec3 surface = texture(roughnessSampler, uv).rgb;   /* r: roughness, g: metallic */
    float roughness = clamp(surface.r, 0.05, 1.0);
    float metallic = clamp(surface.g, 0.0, 1.0);

    vec3 V = normalize(-P);
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * d * d);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G = (NdotV / (NdotV * (1.0 - k) + k)) * (NdotL / (NdotL * (1.0 - k) + k));
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    vec3 specular = D * G * F / (4.0 * NdotV * max(NdotL, 1e-4));
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo;
    /* light colors are authored as "N.L * color" for a diffuse surface, so the
     * Lambert 1/PI is folded into the light and the specular is scaled to match */
    return (diffuse + specular * PI) * radiance * NdotL;
}

void main() {
    vec3 view = vec3(viewRay.xy/-viewRay.z, -1.0);
    vec2 texCoord = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    float depth = 2.0 * texture(depthSampler, texCoord).x - 1.0;
    float eyeZ = projectionMatrix[3][2]/(depth + projectionMatrix[2][2]);
    vec3 viewPosition = view * eyeZ;
    vec3 eyeSpaceNormal = texture (eyeSpaceNormalSampler, texCoord).rgb * 2.0 - vec3(1);
    bool getLight;

    float dotProd = dot(lightDirectionEyeSpace, eyeSpaceNormal);
    float lightIntensity = max(0.0, dotProd);

    vec4 dirLightClip = (dirLightProjMatrix * dirLightViewMatrix * inverseViewMatrix * vec4(viewPosition, 1.0));
    vec3 dirLightNDC = dirLightClip.xyz / dirLightClip.w;
    vec3 dirLightNDCNormalized = dirLightNDC.xyz * 0.5f + 0.5f;

    float eyeZObjectDepth = dirLightNDCNormalized.z;
    float cutOff = 1.0f - eyeZ / maxDist;
    cutOff = clamp(cutOff, 0.0, 1.0);
    float totalPixelsInShadow = 0;
    float lightStrength;

    vec2 depthMapTexelSize = 1.0f / textureSize(dirLightDepthSampler, 0);

    for (int i = -PCFStartingIndex; i <= PCFStartingIndex && castsShadow != 0; i++) {
        for (int j = -PCFStartingIndex; j <= PCFStartingIndex; j++) {
            vec3 blueNoise = texture(blueNoiseSampler, texCoord + vec2(0.1*i, 0.1*j)).rgb;
            vec2 offset = vec2(blueNoise.r, blueNoise.g);

            float currentDepth = texture(dirLightDepthSampler, dirLightNDCNormalized.xy + depthMapTexelSize*vec2(i, j)-
                                offset*0.0005f).x;
            float eyeZODepthMap = currentDepth;

            totalPixelsInShadow += step(eyeZODepthMap, eyeZObjectDepth+0.0005);
        }
    }

    lightStrength = 1.0f - (cutOff*(totalPixelsInShadow / PCFKernelSize));

    vec3 roughness = texture(roughnessSampler, texCoord).rgb;
    float rough = ((roughness.r+roughness.g+roughness.b) / 3.0f);

    vec3 H = normalize(lightDirectionEyeSpace + (-cameraForwardVectorEyeSpace));
    float specularStrength = pow (max (dot(H, eyeSpaceNormal), 0.0), 250.0f);
    getLight = dotProd > 0;

    vec3 diffuseLight = lightIntensity * lightColor;
    vec3 specularLight = specularStrength * lightColor;

    if (pbrEnabled) {
        outLight = pbrShade(texCoord, viewPosition, normalize(eyeSpaceNormal), lightDirectionEyeSpace, lightColor);
    } else {
        outLight = diffuseLight;
        if (getLight)
            outLight += specularLight * rough;
    }

    outLight *= lightStrength;
}
