#version 330

layout(location = 0) out vec3 outLight;

uniform sampler2D eyeSpaceNormalSampler;
uniform sampler2D depthSampler;
uniform sampler2D blueNoiseSampler;
uniform sampler2D spotLightDepthSampler;
uniform sampler2D roughnessSampler;

uniform mat4 spotLightProjectionMatrix;
uniform mat4 spotLightViewMatrix;
uniform mat4 inverseViewMatrix;   /* inverse(viewMatrix), computed on the CPU */
uniform mat4 eyeToSpotLightClip;  /* spotLightProjection * spotLightView * inverseView */

uniform vec3 lightColor;
uniform int screenWidth, screenHeight;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

in vec3 viewRay;
flat in vec3 lightPositionEyeSpace;

uniform vec3 cameraForwardVector;
uniform vec3 cameraForwardVectorEyeSpace;

const int PCFStrength = 1;
const int PCFKernelSideSize = PCFStrength * 2 + 1;
const int PCFStartingIndex = PCFKernelSideSize / 2;
const int PCFKernelSize = PCFKernelSideSize * PCFKernelSideSize;

uniform vec3 lightPosition;
uniform mat4 modelMatrix;

uniform float lightLength;
uniform float alpha;

uniform vec3 att_diffuse;
uniform vec3 att_specular;

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
    bool getLight;
    vec2 texCoord = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    vec3 view = vec3(viewRay.xy/viewRay.z, 1.0);
    float depth = 2.0 * texture(depthSampler, texCoord).x - 1.0;
    float eyeZ = -projectionMatrix[3][2]/(depth + projectionMatrix[2][2]);
    vec3 viewPosition = view * eyeZ;
    vec3 dir = lightPositionEyeSpace - viewPosition;
    float l = length (dir);

    vec4 worldPosition = inverseViewMatrix * vec4(viewPosition, 1.0);
    float spotLightAttCoef = max(0.0, dot (normalize(worldPosition.xyz - lightPosition), normalize(mat3(modelMatrix) * vec3(0,0,-1))));
    float coneFactor = alpha - acos(spotLightAttCoef);
    float lenAtt = max(1.0 - (l / lightLength), 0.0);

    /* Outside the cone or beyond the range the contribution is <= 0, which the
     * UNORM light buffer clamps to 0 anyway: skip the shadow lookups. */
    if (coneFactor <= 0.0 || lenAtt <= 0.0) {
        discard;
    }

    vec3 dirNormalized = normalize (dir);
    float totalPixelsInShadow = 0;
    float lightStrength = 1.0f;

    vec3 eyeSpaceNormal = texture (eyeSpaceNormalSampler, texCoord).rgb * 2.0 - vec3(1);
    float dotProduct = dot (dirNormalized, eyeSpaceNormal);

    vec4 spotLightClip = eyeToSpotLightClip * vec4(viewPosition, 1.0);
    vec3 spotLightNDC = spotLightClip.xyz / spotLightClip.w;
    vec3 spotLightNDCNormalized = spotLightNDC.xyz * 0.5f + 0.5f;

    float eyeZObjectDepth = spotLightProjectionMatrix[3][2]/(spotLightNDCNormalized.z + spotLightProjectionMatrix[2][2]);

    vec2 depthMapTexelSize = 1.0f / textureSize(spotLightDepthSampler, 0);

    for (int i = -PCFStartingIndex; i <= PCFStartingIndex && castsShadow != 0; i++) {
        for (int j = -PCFStartingIndex; j <= PCFStartingIndex; j++) {
            vec3 blueNoise = texture(blueNoiseSampler, texCoord + vec2(0.1*i, 0.1*j)).rgb;
            vec2 offset = vec2(blueNoise.r, blueNoise.g);

            float currentDepth = texture(spotLightDepthSampler, spotLightNDCNormalized.xy + depthMapTexelSize*vec2(i, j)-
                                offset*0.0005f).x;
            float eyeZODepthMap = spotLightProjectionMatrix[3][2]/(currentDepth + spotLightProjectionMatrix[2][2]);

            totalPixelsInShadow += step(eyeZODepthMap, eyeZObjectDepth+0.09);
        }
    }

    lightStrength = 1.0f - (totalPixelsInShadow / PCFKernelSize);

    vec3 H = normalize (dirNormalized + (-cameraForwardVectorEyeSpace));
    getLight = (dotProduct > 0);

    vec3 roughness = texture(roughnessSampler, texCoord).rgb;
    float rough = ((roughness.r+roughness.g+roughness.b) / 3.0f);

    float diffuseStrength = max (0.0, dotProduct);
    float specularStrength = pow (max (dot(H, eyeSpaceNormal), 0.0), 250);

    float a = att_diffuse.x*0.001f, b = att_diffuse.y * 0.000001f, c = att_diffuse.z * 0.000001f;
    float att = 1.0 / (a + b*l + c * l * l);

    float a2 = att_specular.x*0.001f, b2 = att_specular.y * 0.000001f, c2 = att_specular.z * 0.000001f;
    float att2 = 1.0 / (a2 + b2*l + c2 * l * l);

    vec3 diffuseLight = coneFactor * att * lenAtt * diffuseStrength * lightColor;
    vec3 specularLight = coneFactor * att2 * lenAtt * specularStrength * lightColor;

    if (pbrEnabled) {
        outLight = pbrShade(texCoord, viewPosition, normalize(eyeSpaceNormal), dirNormalized,
                            coneFactor * att * lenAtt * lightColor);
    } else {
        outLight = diffuseLight;
        if (getLight) {
            outLight += specularLight * rough;
        }
    }

    outLight *= lightStrength;
}
