#version 330

layout(location = 0) out vec3 outLight;

uniform sampler2D eyeSpaceNormalSampler;
uniform sampler2D depthSampler;
uniform sampler2D roughnessSampler;

uniform vec3 lightColor;
uniform int screenWidth, screenHeight;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

in vec3 viewRay;
flat in vec3 lightPositionEyeSpace;

uniform vec3 cameraForwardVector;
uniform vec3 cameraForwardVectorEyeSpace;

uniform float cutOff;

uniform vec3 att_diffuse;
uniform vec3 att_specular;

uniform vec3 lightPosition;      /* world space */
uniform int castsShadow;
uniform float shadowFar;
uniform samplerCube shadowCube;  /* distance to the light / shadowFar */
uniform mat4 inverseViewMatrix;

float pointShadow(vec3 viewPosition, vec3 viewNormal, float nDotL) {
    vec3 worldPosition = (inverseViewMatrix * vec4(viewPosition, 1.0)).xyz;
    vec3 worldNormal = normalize(mat3(inverseViewMatrix) * viewNormal);
    float sinAngle = sqrt(max(1.0 - nDotL * nDotL, 0.0));
    /* normal offset: look the shadow up a little off the surface, more at
     * grazing angles (a ceiling right above a tube), where the depth bias
     * alone fails past some distance and cuts the light off in a hard ring.
     * It is larger than the sampling disk, so no tap lands behind the surface */
    float d0 = length(worldPosition - lightPosition);
    vec3 toFragment = worldPosition + worldNormal * (0.5 + d0 * 0.016) * sinAngle - lightPosition;
    float d = length(toFragment);
    /* slope-scaled: surfaces lit at grazing angles need more bias (no acne) */
    float slope = sinAngle / max(nDotL, 0.1);
    float bias = 0.5 + d * (0.006 + 0.012 * min(slope, 6.0));
    float disk = d * 0.008;
    const vec3 offsets[4] = vec3[](vec3(1, 1, 1), vec3(-1, -1, 1), vec3(-1, 1, -1), vec3(1, -1, -1));
    float lit = 0.0;
    for (int i = 0; i < 4; i++) {
        float closest = texture(shadowCube, toFragment + offsets[i] * disk).r * shadowFar;
        lit += (d - bias <= closest) ? 1.0 : 0.0;
    }
    return lit * 0.25;
}


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
    vec3 F = F0 + (1.0 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);

    vec3 specular = D * G * F / (4.0 * NdotV * max(NdotL, 1e-4));
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo;
    /* light colors are authored as "N.L * color" for a diffuse surface, so the
     * Lambert 1/PI is folded into the light and the specular is scaled to match */
    return (diffuse + specular * PI) * radiance * NdotL;
}

void main() {
    bool getLight;
    vec2 texCoord = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    vec3 view = vec3(viewRay.xy/-viewRay.z, -1.0);
    float depth = 2.0 * texture(depthSampler, texCoord).x - 1.0;
    float eyeZ = projectionMatrix[3][2]/(depth + projectionMatrix[2][2]);
    vec3 viewPosition = view * eyeZ;
    vec3 dir = lightPositionEyeSpace - viewPosition;
    vec3 dirNormalized = normalize (dir);
    float l = length (dir);

    if (l - cutOff > 0.001) {
        discard;
    }
    vec3 eyeSpaceNormal = texture (eyeSpaceNormalSampler, texCoord).rgb * 2.0 - vec3(1);
    float dotProduct = dot (dirNormalized, eyeSpaceNormal);

    vec3 H = normalize (dirNormalized + (-cameraForwardVectorEyeSpace));
    getLight = (dotProduct > 0);

    vec3 roughness = texture(roughnessSampler, texCoord).rgb;
    float rough = ((roughness.r+roughness.g+roughness.b) / 3.0f);

    float diffuseStrength = max(0.0, dotProduct);
    float specularStrength = pow(max(dot(H, eyeSpaceNormal), 0.0), 250);

    float a = att_diffuse.x*0.001f, b = att_diffuse.y * 0.000001f, c = att_diffuse.z * 0.000001f;
    float att = 1.0 / (a + b*l + c * l * l);

    float a2 = att_specular.x*0.001f, b2 = att_specular.y * 0.000001f, c2 = att_specular.z * 0.000001f;
    float att2 = 1.0 / (a2 + b2*l + c2 * l * l);

    /* linear fade to the cut-off, eased flat at the end (no visible ring
     * where the light's range stops) */
    float x = clamp(l / cutOff, 0.0, 1.0);
    float lenAtt = (1.0 - x) * (1.0 - pow(x, 8.0));

    vec3 diffuseLight = att * diffuseStrength * lightColor * lenAtt;
    vec3 specularLight = att2 * specularStrength * lightColor * lenAtt;

    if (pbrEnabled) {
        outLight = pbrShade(texCoord, viewPosition, normalize(eyeSpaceNormal), dirNormalized,
                            att * lenAtt * lightColor);
    } else {
        outLight = diffuseLight;
        if (getLight) {
            outLight += specularLight * rough;
        }
    }
    if (castsShadow != 0) {
        outLight *= pointShadow(viewPosition, normalize(eyeSpaceNormal), max(dotProduct, 0.0));
    }
}
