#version 330

layout(points) in;
layout(triangle_strip, max_vertices = 6) out;

uniform mat4 projectionMatrix, viewMatrix;
out vec2 previousTextCoords;
out vec2 textCoords;

in vec3 eyeSpaceNormal[1];
in mat4 viewModelMatrixOut[1];
in float outAliveForInMs[1];
out vec3 normal;
out float blendFactor;

uniform int liveForInMs;
uniform int totalNumOfSubTxts;
uniform int numOfSubTxtsH, numOfSubTxtsW;
uniform float subWidth, subHeight;

void main(){
    mat4 PVM = projectionMatrix * viewModelMatrixOut[0];
    vec4 ul = PVM * vec4(vec3(-0.5, 0.5, 0), 1.0);
    vec4 ur = PVM * vec4(vec3(0.5, 0.5, 0), 1.0);
    vec4 ll = PVM * vec4(vec3(-0.5, -0.5, 0), 1.0);
    vec4 lr = PVM * vec4(vec3(0.5, -0.5, 0), 1.0);

    float normalizedAliveTime = (outAliveForInMs[0] / liveForInMs) * (totalNumOfSubTxts - 1);

    vec4 currentTextCoords, prevTxtCoords;
    float txtIndex;
    blendFactor = modf (normalizedAliveTime, txtIndex);
    int rowOffset, columnOffset;

    rowOffset = int (floor (txtIndex / numOfSubTxtsH));
    columnOffset = int (txtIndex) - rowOffset * numOfSubTxtsH;

    currentTextCoords.x = float (columnOffset) * subWidth;
    currentTextCoords.y = float (rowOffset) * subHeight;
    currentTextCoords.z = float (columnOffset + 1) * subWidth;
    currentTextCoords.w = float (rowOffset + 1) * subHeight;

    if (txtIndex == totalNumOfSubTxts - 1) {
        prevTxtCoords = currentTextCoords;
    } else {
        txtIndex++;
        rowOffset = int (floor (txtIndex / numOfSubTxtsH));
    	columnOffset = int (txtIndex) - rowOffset * numOfSubTxtsH;

    	prevTxtCoords.x = float (columnOffset) * subWidth;
    	prevTxtCoords.y = float (rowOffset) * subHeight;
        prevTxtCoords.z = float (columnOffset + 1) * subWidth;
    	prevTxtCoords.w = float (rowOffset + 1) * subHeight;
    }

    gl_Position = ul;
    textCoords = vec2(currentTextCoords.x, currentTextCoords.y);
    previousTextCoords = vec2(prevTxtCoords.x, prevTxtCoords.y);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    gl_Position = ll;
    textCoords = vec2(currentTextCoords.x, currentTextCoords.w);
    previousTextCoords = vec2(prevTxtCoords.x, prevTxtCoords.w);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    gl_Position = ur;
    textCoords = vec2(currentTextCoords.z, currentTextCoords.y);
    previousTextCoords = vec2(prevTxtCoords.z, prevTxtCoords.y);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    EndPrimitive();

    gl_Position = ur;
    textCoords = vec2(currentTextCoords.z, currentTextCoords.y);
    previousTextCoords = vec2(prevTxtCoords.z, prevTxtCoords.y);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    gl_Position = ll;
    textCoords = vec2(currentTextCoords.x, currentTextCoords.w);
    previousTextCoords = vec2(prevTxtCoords.x, prevTxtCoords.w);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    gl_Position = lr;
    textCoords = vec2(currentTextCoords.z, currentTextCoords.w);
    previousTextCoords = vec2(prevTxtCoords.z, prevTxtCoords.w);
    normal = eyeSpaceNormal[0];
    EmitVertex();
    EndPrimitive();
}
