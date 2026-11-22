#version 330

/* Mirrored-camera pass for water reflections (forward lit, clipped at the
 * water plane). Handles plain and instanced meshes. */

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 5) in mat4 instanceModelMatrix;

uniform mat4 modelMatrix, viewMatrix, projectionMatrix;
uniform bool instanced;
uniform float clipHeight;

out vec3 worldPosition;
out vec3 worldNormal;
out vec2 textureCoords;

void main() {
    mat4 model = instanced ? instanceModelMatrix : modelMatrix;
    vec4 world = model * vec4(inPosition, 1.0);
    worldPosition = world.xyz;
    worldNormal = transpose(inverse(mat3(model))) * inNormal;
    textureCoords = inTexcoord;
    gl_ClipDistance[0] = world.y - clipHeight;
    gl_Position = projectionMatrix * viewMatrix * world;
}
