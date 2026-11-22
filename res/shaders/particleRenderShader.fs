#version 330

layout(location = 0) out vec4 outColor;

in vec2 textCoords;
in vec2 previousTextCoords;
in float blendFactor;
in float aliveFor;
flat in float particleIntensity;

uniform sampler2D particleSampler;
uniform sampler2D depthSampler;
uniform mat3 fromTangentToModelSpace;
uniform int screenWidth, screenHeight;

uniform vec3 tint;           /* color multiplier */
uniform float brightnessAdd; /* added to the texture color (smoke: 0.25) */
uniform int additive;        /* 1: additive glow (fire, fireflies) */
uniform float fadeIn;        /* fraction of the lifetime spent fading in */
uniform float opacity;       /* whole emitter (night-only alpha emitters fade by day) */

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;

uniform float fogStart, fogEnd;
/* distance cut-off: 0 before fogStart, closes in exponentially and is
 * exactly 1 (fully fogged) at fogEnd, the far plane. fogEnd 0 = off */
float fogCutoff(float dist) {
    if (fogEnd <= 0.0) return 0.0;
    float d = clamp((dist - fogStart) / max(fogEnd - fogStart, 1.0), 0.0, 1.0);
    return (1.0 - exp(-4.0 * d * d)) / (1.0 - exp(-4.0));
}

/* soft particles */
#define EPS 40

void main(){
    vec2 texCoord = gl_FragCoord.xy / (vec2(screenWidth, screenHeight)/2);
    vec4 textColor = texture(particleSampler, textCoords);
    vec4 prevTextColor = texture(particleSampler, previousTextCoords);

    /* the particle buffer is half resolution: each of its pixels covers 2x2
     * scene pixels. Test against the farthest of them, or at silhouettes
     * (lamp globes) the glow is dropped where it should show, leaving a dark
     * dotted ring around the object */
    vec2 sceneTexel = 0.5 / vec2(screenWidth, screenHeight);
    float depth = max(max(texture(depthSampler, texCoord + vec2(-sceneTexel.x, -sceneTexel.y)).r,
                          texture(depthSampler, texCoord + vec2(sceneTexel.x, -sceneTexel.y)).r),
                      max(texture(depthSampler, texCoord + vec2(-sceneTexel.x, sceneTexel.y)).r,
                          texture(depthSampler, texCoord + vec2(sceneTexel.x, sceneTexel.y)).r));
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
    outColor.a *= envelope * particleIntensity * opacity;
    /* particles are composited over the fogged scene: fade them with the
     * same cut-off (distance along the view axis) */
    if (fogEnd > 0.0) {
        float viewDepth = projectionMatrix[3][2] / (gl_FragCoord.z * 2.0 - 1.0 + projectionMatrix[2][2]);
        outColor.a *= 1.0 - fogCutoff(abs(viewDepth));
    }
    outColor.xyz = (outColor.xyz + vec3(brightnessAdd)) * tint;
    outColor.xyz *= outColor.a;
    /* premultiplied: alpha 0 means "add, don't occlude" in the composite */
    if (additive != 0) {
        outColor.a = 0.0;
    }
}
