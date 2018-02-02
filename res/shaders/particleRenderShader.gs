#version 330

layout(points) in;
layout(triangle_strip, max_vertices = 6) out;

uniform mat4 projectionMatrix, viewMatrix;
out vec2 textCoords;

in vec3 eyeSpaceNormal[1];
in mat4 viewModelMatrixOut[1];
out vec3 normal;

void main(){
    mat4 PVM = projectionMatrix * viewModelMatrixOut[0];
    vec4 ul = PVM * vec4(vec3(-0.5, 0.5, 0), 1.0);
    vec4 ur = PVM * vec4(vec3(0.5, 0.5, 0), 1.0);
    vec4 ll = PVM * vec4(vec3(-0.5, -0.5, 0), 1.0);
    vec4 lr = PVM * vec4(vec3(0.5, -0.5, 0), 1.0);

    gl_Position = ul;
    textCoords = vec2(0, 0);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    gl_Position = ll;
    textCoords = vec2(0, 1);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    gl_Position = ur;
    textCoords = vec2(1, 0);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    EndPrimitive();

    gl_Position = ur;
    textCoords = vec2(1, 0);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    gl_Position = ll;
    textCoords = vec2(0, 1);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    gl_Position = lr;
    textCoords = vec2(1, 1);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    EndPrimitive();
}
