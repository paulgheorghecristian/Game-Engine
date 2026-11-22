#version 330

layout(location = 0) in vec3 inPosition;

uniform mat4 modelMatrix, viewMatrix, projectionMatrix;
uniform float time;
uniform float tiling;
uniform float waveHeight;

out vec3 worldPosition;
out vec2 waveCoords;

void main() {
    vec4 world = modelMatrix * vec4(inPosition, 1.0);
    /* gentle swell; the detailed motion comes from the normal / DuDv maps */
    world.y += waveHeight * (sin(world.x * 0.021 + time * 0.9) * 0.6 +
                             sin(world.z * 0.017 - time * 0.7) * 0.4);
    worldPosition = world.xyz;
    waveCoords = world.xz / tiling;
    gl_Position = projectionMatrix * viewMatrix * world;
}
