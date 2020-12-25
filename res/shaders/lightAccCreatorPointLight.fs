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
    float specularStrength = pow(max(dot(H, eyeSpaceNormal), 0.0), 500);

    float a = att_diffuse.x*0.001f, b = att_diffuse.y * 0.000001f, c = att_diffuse.z * 0.000001f;
    float att = 1.0 / (a + b*l + c * l * l);

    float a2 = att_specular.x*0.001f, b2 = att_specular.y * 0.000001f, c2 = att_specular.z * 0.000001f;
    float att2 = 1.0 / (a2 + b2*l + c2 * l * l);

    float lenAtt = max(1.0 - (l / cutOff), 0.0);

    vec3 diffuseLight = att * diffuseStrength * lightColor * lenAtt;
    vec3 specularLight = att2 * specularStrength * lightColor * lenAtt;

    outLight = diffuseLight;
    if (getLight) {
        outLight += specularLight * rough;
    }
}
