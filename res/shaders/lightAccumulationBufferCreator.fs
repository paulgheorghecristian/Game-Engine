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

uniform float cutOff; /* cutOff negative means spot light */
uniform vec3 cameraForwardVector;

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

    if (cutOff > 0 && l - cutOff > 0.001) {
        discard;
    }
    vec3 eyeSpaceNormal = texture (eyeSpaceNormalSampler, texCoord).rgb * 2.0 - vec3(1);
    float dotProduct = dot (dirNormalized, eyeSpaceNormal);

    if (cutOff < 0) {
        /* spot light */
        mat4 inverseViewMatrix = inverse (viewMatrix);

        vec4 worldPosition = (inverseViewMatrix * vec4(viewPosition, 1.0));
        vec4 spotLightEyePosition = (spotLightViewMatrix * worldPosition);
        vec4 spotLightClip = (spotLightProjectionMatrix * spotLightViewMatrix * inverseViewMatrix * vec4(viewPosition, 1.0));
        vec3 spotLightNDC = spotLightClip.xyz / spotLightClip.w;
        vec3 spotLightNDCNormalized = spotLightNDC.xyz * 0.5f + 0.5f;

        float currentDepth = texture (spotLightDepthSampler, spotLightNDCNormalized.xy).x;
        if (currentDepth < spotLightNDCNormalized.z) {
            discard;
        }
    }

    vec3 H = normalize (dirNormalized + normalize (-cameraForwardVector));
    getLight = (dotProduct > 0);

    float diffuseStrength = max (0.0, dotProduct);
    float specularStrength = pow (max (dot(H, eyeSpaceNormal), 0.0), 50.0);

    float a = 1.0, b = 0.007, c = 0.0002;
    float att = 1.0 / (a + b*l + c * l * l);

    vec3 diffuseLight = att * diffuseStrength * lightColor;
    vec3 specularLight = att * specularStrength * lightColor;

    outLight = diffuseLight;
    if (getLight) {
        outLight += specularLight;
    }
}
