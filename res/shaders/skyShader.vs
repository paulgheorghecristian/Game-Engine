#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords;

out vec2 texCoords;
out vec3 skyDirection;

uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
uniform mat4 projectionMatrix;
uniform int farPlane;   /* 1: at the far plane (drawn after the scene, only where nothing else is) */

void main(){
    texCoords = inTexCoords;
    /* direction from the camera (the dome is offset from it), so the sun /
     * moon / stars line up with their real directions */
    skyDirection = (modelMatrix * vec4(inPosition, 1)).xyz;
    gl_Position = projectionMatrix * mat4(mat3(viewMatrix)) * modelMatrix *  vec4(inPosition, 1);
    if (farPlane != 0) gl_Position.z = gl_Position.w;
}
