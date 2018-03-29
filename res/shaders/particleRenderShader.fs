#version 330

layout(location = 0) out vec4 outColor;

in vec2 textCoords;
in vec2 previousTextCoords;
in float blendFactor;
in float aliveFor;

uniform sampler2D particleSampler;
uniform sampler2D depthSampler;
uniform mat3 fromTangentToModelSpace;
uniform int screenWidth, screenHeight;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;

#define EPS 20

void main(){
    vec2 texCoord = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    vec4 textColor = texture(particleSampler, textCoords);
    vec4 prevTextColor = texture(particleSampler, previousTextCoords);

    float depth = texture (depthSampler, texCoord).r;
    float eyeZ = projectionMatrix[3][2]/(depth + projectionMatrix[2][2]);

    float depth2 = gl_FragCoord.z;
    float eyeZ2 = projectionMatrix[3][2]/(depth2 + projectionMatrix[2][2]);

    float dif = eyeZ - eyeZ2;

    if (dif < 0) {
        discard;
    }

    outColor = mix (textColor, prevTextColor, blendFactor);

    if (dif < EPS) {
        outColor.a *= (dif / EPS);
    }

    outColor.a *= (1.0 - aliveFor);
    outColor.xyz += vec3(0.5,0.5,0.25) * 0.7;
    outColor.xyz *= outColor.a;
}
