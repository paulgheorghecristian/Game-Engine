#version 330

layout(location = 0) out vec3 outLight;

uniform sampler2D eyeSpaceNormalSampler;
uniform sampler2D depthSampler;
uniform sampler2D spotLightDepthSampler;

uniform mat4 spotLightProjectionMatrix;
uniform mat4 spotLightViewMatrix;

uniform vec3 lightColor;
uniform int screenWidth, screenHeight;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

in vec3 viewRay;
flat in vec3 lightPositionEyeSpace;

uniform vec3 cameraForwardVector;
uniform vec3 cameraForwardVectorEyeSpace;

const int PCFStrength = 2;
const int PCFKernelSideSize = PCFStrength * 2 + 1;
const int PCFStartingIndex = PCFKernelSideSize / 2;
const int PCFKernelSize = PCFKernelSideSize * PCFKernelSideSize;
const float depthMapTexelSize = 1.0f/2048.0f; /* TODO remove hardcode */

uniform vec3 lightPosition;
uniform mat4 modelMatrix;

const float lightRadius = 300;
const float fact = 0.5;

void main() {
    bool getLight;
    vec2 texCoord = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    vec3 view = vec3(viewRay.xy/viewRay.z, 1.0);
    float depth = 2.0 * texture(depthSampler, texCoord).x - 1.0;
    float eyeZ = -projectionMatrix[3][2]/(depth + projectionMatrix[2][2]);
    vec3 viewPosition = view * eyeZ;
    vec3 dir = lightPositionEyeSpace - viewPosition;
    vec3 dirNormalized = normalize (dir);
    float l = length (dir);
    float totalPixelsInShadow = 0;
    float lightStrength = 1.0f;

    vec3 eyeSpaceNormal = texture (eyeSpaceNormalSampler, texCoord).rgb * 2.0 - vec3(1);
    float dotProduct = dot (dirNormalized, eyeSpaceNormal);
    float spotLightAttCoef;


    //TODO uniform instead of calculate here
    mat4 inverseViewMatrix = inverse (viewMatrix);

    // TODO investigate how to move this to VS
    vec4 worldPosition = (inverseViewMatrix * vec4(viewPosition, 1.0));
    vec4 spotLightClip = (spotLightProjectionMatrix * spotLightViewMatrix * worldPosition);
    vec3 spotLightNDC = spotLightClip.xyz / spotLightClip.w;
    vec3 spotLightNDCNormalized = spotLightNDC.xyz * 0.5f + 0.5f;

    float eyeZObjectDepth = spotLightProjectionMatrix[3][2]/(spotLightNDCNormalized.z + spotLightProjectionMatrix[2][2]);

    for (int i = -PCFStartingIndex; i <= PCFStartingIndex; i++) {
        for (int j = -PCFStartingIndex; j <= PCFStartingIndex; j++) {
            float currentDepth = texture (spotLightDepthSampler, spotLightNDCNormalized.xy + vec2(depthMapTexelSize*i, depthMapTexelSize*j)).x;
            float eyeZODepthMap = spotLightProjectionMatrix[3][2]/(currentDepth + spotLightProjectionMatrix[2][2]);

            if (eyeZODepthMap < eyeZObjectDepth + 0.9) {
                totalPixelsInShadow++;
            }
        }
    }

    lightStrength = 1.0f - (totalPixelsInShadow / PCFKernelSize);
    spotLightAttCoef = max(0.0, dot (normalize(worldPosition.xyz - lightPosition), normalize(mat3(modelMatrix) * vec3(0,0,-1))));

    vec3 H = normalize (dirNormalized + (-cameraForwardVectorEyeSpace));
    getLight = (dotProduct > 0);

    float diffuseStrength = max (0.0, dotProduct);
    float specularStrength = pow (max (dot(H, eyeSpaceNormal), 0.0), 1000.0);

    float a = 0.0, b = 0.0001, c = 0.000001;
    float att = 1.0 / (a + b*l + c * l * l);
    float lenAtt = 1.0f;//max(1.0 - (l / lightRadius), 0);

    vec3 diffuseLight = (0.3 - acos(spotLightAttCoef)) * att * lenAtt * diffuseStrength * lightColor;
    vec3 specularLight = (0.3 - acos(spotLightAttCoef)) * att * lenAtt * specularStrength * lightColor;

    outLight = diffuseLight*fact;
    if (getLight) {
        outLight += specularLight*(1.0-fact);
    }

    outLight *= lightStrength;
}
