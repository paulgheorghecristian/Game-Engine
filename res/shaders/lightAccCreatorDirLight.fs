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

/* the crowns (trees' leaves) as ellipsoids, two vec4 each: (world centre, horizontal radius),
 * (how dark their shadow is, horizontal / vertical radius, -, -) */
uniform vec4 crowns[128];
uniform int crownCount;

/* how much of the light reaches world (towards the light L) past the crowns: soft shadows cast
 * along the light, so they lie where the sun puts them and stretch as it sinks. Not on the
 * crown itself (its own shading does that) */
float crownLight(vec3 world, vec3 L) {
    float lit = 1.0;
    for (int i = 0; i < crownCount; i++) {
        vec4 sphere = crowns[i * 2], shape = crowns[i * 2 + 1];
        /* squashed so the ellipsoid is a sphere */
        vec3 squash = vec3(1.0, shape.y, 1.0);
        vec3 rel = (sphere.xyz - world) * squash;
        vec3 dir = normalize(L * squash);
        float along = dot(rel, dir);
        if (along <= 0.0) continue;
        float r = sphere.w;
        float miss = length(rel - dir * along);
        float outside = smoothstep(r * 0.95, r * 1.25, length(rel));
        lit *= 1.0 - shape.x * smoothstep(r, r * 0.3, miss) * outside;
    }
    return lit;
}


/* ---- PBR (Cook-Torrance: GGX distribution, Smith-Schlick visibility,
 * Schlick Fresnel; metal/roughness workflow) ---- */
/* ---- clouds: one drifting fbm layer cloudHeight above the world's zero (world units); the
 * sky draws it and the directional light pass uses the same density for the
 * cloud shadows on the ground, so the two always match. Higher clouds are larger
 * and drift faster in proportion, so from a standstill the sky looks the same at
 * any height: the height only sets how much they slide past as the camera moves ---- */
uniform float cloudCoverage;   /* 0: no clouds .. 1: overcast */
uniform vec2 cloudWind;        /* drift in world units per second, for clouds at 1500 */
uniform float cloudTime;       /* seconds */
uniform float cloudHeight;     /* world units (0: not set, 1500) */
float cloudScale() { return cloudHeight > 0.0 ? cloudHeight / 1500.0 : 1.0; }

float cloudHash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float cloudNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(cloudHash(i), cloudHash(i + vec2(1, 0)), u.x),
               mix(cloudHash(i + vec2(0, 1)), cloudHash(i + vec2(1, 1)), u.x), u.y);
}

/* 0..1 cloud density above world position xz */
float cloudDensity(vec2 xz) {
    vec2 p = (xz / cloudScale() + cloudWind * cloudTime) * 0.0009;
    float n = 0.0, amp = 0.5;
    mat2 rot = mat2(0.8, -0.6, 0.6, 0.8);
    for (int i = 0; i < 5; i++) {
        n += amp * cloudNoise(p);
        p = rot * p * 2.07 + vec2(7.3, 1.9);
        amp *= 0.5;
    }
    float threshold = mix(0.68, 0.3, cloudCoverage);
    return smoothstep(threshold, threshold + 0.22, n);
}

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
    vec3 F = F0 + (1.0 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);

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
    /* past the shadow map (it fades out by maxDist, and covers only so much round the view): the
     * crowns' soft shadows instead, faded in as it fades out */
    if (crownCount > 0) {
        bool mapped = castsShadow != 0 && all(greaterThanEqual(dirLightNDCNormalized, vec3(0.0))) &&
                      all(lessThanEqual(dirLightNDCNormalized, vec3(1.0)));
        float far = 1.0 - (mapped ? cutOff : 0.0);
        if (far > 0.0) {
            vec3 world = (inverseViewMatrix * vec4(viewPosition, 1.0)).xyz;
            lightStrength *= mix(1.0, crownLight(world, normalize(lightDirection)), far);
        }
    }

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
    /* cloud shadows: follow the light up to the cloud layer */
    if (cloudCoverage > 0.0) {
        vec3 world = (inverseViewMatrix * vec4(viewPosition, 1.0)).xyz;
        vec3 L = normalize(lightDirection);
        vec2 atClouds = world.xz + L.xz / max(L.y, 0.1) * (1500.0 * cloudScale() - world.y);
        outLight *= 1.0 - 0.72 * cloudDensity(atClouds);
    }
}
