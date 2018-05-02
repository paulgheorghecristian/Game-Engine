#version 330

layout(location = 0) out vec3 outLight;

uniform vec3 lightColor, lightDirection;
uniform int screenWidth, screenHeight;
uniform sampler2D dirLightDepthSampler;
uniform sampler2D eyeSpaceNormalSampler;
uniform sampler2D depthSampler;
uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;

uniform mat4 dirLightProjMatrix, dirLightViewMatrix;

in vec3 viewRay;

const int PCFStrength = 3;
const int PCFKernelSideSize = PCFStrength * 2 + 1;
const int PCFStartingIndex = PCFKernelSideSize / 2;
const int PCFKernelSize = PCFKernelSideSize * PCFKernelSideSize;
const float depthMapTexelSize = 1.0f/2048.0f; /* TODO remove hardcode */

/* TODO remove hardcode */
const float maxDist = 500.0f;

void main() {
    vec3 view = vec3(viewRay.xy/-viewRay.z, -1.0);
    vec2 texCoord = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    float depth = 2.0 * texture(depthSampler, texCoord).x - 1.0;
    float eyeZ = projectionMatrix[3][2]/(depth + projectionMatrix[2][2]);
    vec3 viewPosition = view * eyeZ;
    vec3 eyeSpaceNormal = texture (eyeSpaceNormalSampler, texCoord).rgb * 2.0 - vec3(1);
    mat4 inverseViewMatrix = inverse(viewMatrix);

    float lightIntensity = max(0.2, dot(normalize(mat3(viewMatrix) * -lightDirection), eyeSpaceNormal));

    vec4 dirLightClip = (dirLightProjMatrix * dirLightViewMatrix * inverseViewMatrix * vec4(viewPosition, 1.0));
    vec3 dirLightNDC = dirLightClip.xyz / dirLightClip.w;
    vec3 dirLightNDCNormalized = dirLightNDC.xyz * 0.5f + 0.5f;

    float eyeZObjectDepth = -1.0f/(dirLightNDCNormalized.z);
    float cutOff = 1.0f - eyeZ / maxDist;
    cutOff = clamp(cutOff, 0.0, 1.0);
    float totalPixelsInShadow = 0;
    float lightStrength;

    for (int i = -PCFStartingIndex; i <= PCFStartingIndex; i++) {
        for (int j = -PCFStartingIndex; j <= PCFStartingIndex; j++) {
            float currentDepth = texture (dirLightDepthSampler, dirLightNDCNormalized.xy + vec2(depthMapTexelSize*i, depthMapTexelSize*j)).x;
            float eyeZODepthMap = -1.0f/(currentDepth);

            if (eyeZODepthMap < eyeZObjectDepth) {
                totalPixelsInShadow++;
            }
        }
    }

    lightStrength = 1.0f - (cutOff*(totalPixelsInShadow / PCFKernelSize));

    outLight = lightIntensity * lightColor * lightStrength;
}
