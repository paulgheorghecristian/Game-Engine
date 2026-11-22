#version 330

/* Point light shadow cube face (plain and instanced meshes). */

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inTexcoord;
layout(location = 5) in mat4 instanceModelMatrix;

uniform mat4 modelMatrix, viewProjection;
uniform bool instanced;

out vec3 worldPosition;
out vec2 textureCoords;

void main() {
    vec4 world = (instanced ? instanceModelMatrix : modelMatrix) * vec4(inPosition, 1.0);
    worldPosition = world.xyz;
    textureCoords = inTexcoord;
    gl_Position = viewProjection * world;
}
