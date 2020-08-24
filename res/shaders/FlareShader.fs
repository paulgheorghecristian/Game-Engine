#version 330

layout(location = 0) out vec4 outColor;

uniform mat4 projectionMatrix;

uniform sampler2D depthSampler;
uniform int screenWidth, screenHeight;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

uniform Material material;

void main(){
    vec2 texCoord = gl_FragCoord.xy / (vec2(screenWidth, screenHeight));

    float depth = texture(depthSampler, texCoord).r;
    float eyeZ = projectionMatrix[3][2]/(depth + projectionMatrix[2][2]);

    float depth2 = gl_FragCoord.z;
    float eyeZ2 = projectionMatrix[3][2]/(depth2 + projectionMatrix[2][2]);

    float dif = eyeZ - eyeZ2;

    if (dif < 0) {
        discard;
    }

    outColor.a = 0.4;
    outColor.xyz = material.ambient;
}
