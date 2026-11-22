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

uniform vec3 tint;           /* color multiplier */
uniform float brightnessAdd; /* added to the texture color (smoke: 0.25) */
uniform int additive;        /* 1: additive glow (fire, fireflies) */
uniform float fadeIn;        /* fraction of the lifetime spent fading in */

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;

/* soft particles */
#define EPS 40

void main(){
    vec2 texCoord = gl_FragCoord.xy / (vec2(screenWidth, screenHeight)/2);
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

    float envelope = 1.0 - aliveFor;
    if (fadeIn > 0.0) {
        envelope *= clamp(aliveFor / fadeIn, 0.0, 1.0);
    }
    outColor.a *= envelope;
    outColor.xyz = (outColor.xyz + vec3(brightnessAdd)) * tint;
    outColor.xyz *= outColor.a;
    /* premultiplied: alpha 0 means "add, don't occlude" in the composite */
    if (additive != 0) {
        outColor.a = 0.0;
    }
}
