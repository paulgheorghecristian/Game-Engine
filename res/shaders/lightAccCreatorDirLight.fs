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
            vec3 blueNoise = texture(blueNoiseSampler, texCoord + vec2(0.1*i, 0.1*j)).rgb;
            vec2 offset = vec2(blueNoise.r, blueNoise.g);

            float currentDepth = texture(dirLightDepthSampler, dirLightNDCNormalized.xy + vec2(depthMapTexelSize*i, depthMapTexelSize*j)-
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

    outLight = diffuseLight;
    if (getLight)
        outLight += specularLight * rough;

    outLight *= lightStrength;
}
