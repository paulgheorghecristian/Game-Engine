#version 330

layout(location = 0) out vec3 outLight;

uniform vec3 lightColor, lightDirection;
uniform int screenWidth, screenHeight;
uniform sampler2D dirLightDepthSampler;
uniform sampler2D eyeSpaceNormalSampler;
uniform sampler2D depthSampler;
uniform sampler2D roughnessSampler;
uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform vec3 cameraForwardVector;
uniform vec3 cameraForwardVectorEyeSpace;

uniform mat4 dirLightProjMatrix, dirLightViewMatrix;

in vec3 viewRay;

const int PCFStrength = 2;
const int PCFKernelSideSize = PCFStrength * 2 + 1;
const int PCFStartingIndex = PCFKernelSideSize / 2;
const int PCFKernelSize = PCFKernelSideSize * PCFKernelSideSize;
const float depthMapTexelSize = 1.0f/2048.0f; /* TODO remove hardcode */

/* TODO remove hardcode */
const float maxDist = 300.0f;

flat in vec3 lightDirectionEyeSpace;

const float fac = 0.5;

void main() {
    vec3 view = vec3(viewRay.xy/-viewRay.z, -1.0);
    vec2 texCoord = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    float depth = 2.0 * texture(depthSampler, texCoord).x - 1.0;
    float eyeZ = projectionMatrix[3][2]/(depth + projectionMatrix[2][2]);
    vec3 viewPosition = view * eyeZ;
    vec3 eyeSpaceNormal = texture (eyeSpaceNormalSampler, texCoord).rgb * 2.0 - vec3(1);
    mat4 inverseViewMatrix = inverse(viewMatrix);
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

    for (int i = -PCFStartingIndex; i <= PCFStartingIndex; i++) {
        for (int j = -PCFStartingIndex; j <= PCFStartingIndex; j++) {
            float currentDepth = texture (dirLightDepthSampler, dirLightNDCNormalized.xy + vec2(depthMapTexelSize*i, depthMapTexelSize*j)).x;
            float eyeZODepthMap = currentDepth;

            if (eyeZODepthMap < eyeZObjectDepth) {
                totalPixelsInShadow++;
            }
        }
    }

    lightStrength = 1.0f - (cutOff*(totalPixelsInShadow / PCFKernelSize));

    vec3 roughness = texture(roughnessSampler, texCoord).rgb;
    float rough = ((roughness.r+roughness.g+roughness.b) / 3.0f);
    float specFactor = max(0.0f,250.0f - 50.0f*rough);

    vec3 H = normalize(lightDirectionEyeSpace + (-cameraForwardVectorEyeSpace));
    float specularStrength = pow (max (dot(H, eyeSpaceNormal), 0.0), specFactor);
    getLight = dotProd > 0;

    vec3 diffuseLight = lightIntensity * lightColor * (1.0f-rough);
    vec3 specularLight = specularStrength * lightColor * rough;

    outLight = diffuseLight;
    if (getLight)
        outLight += specularLight;

    outLight *= lightStrength;
}
