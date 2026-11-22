#version 330

layout(points) in;
layout(triangle_strip, max_vertices = 6) out;

uniform mat4 projectionMatrix, viewMatrix;
out vec2 previousTextCoords;
out vec2 textCoords;

in vec3 eyeSpaceNormal[1];
in mat4 viewModelMatrixOut[1];
in float outAliveForInMs[1];
out float blendFactor;
out float aliveFor;

uniform int liveForInMs;
uniform int totalNumOfSubTxts;
uniform int numOfSubTxtsH, numOfSubTxtsW;
uniform float subWidth, subHeight;
/* 1: atlas frames are ordered from the top-left of the image (textures are
 * loaded flipped, so rows are counted from v = 1 downwards) */
uniform int atlasTopDown;

vec4 cellCoords(float index) {
    int row = int(floor(index / numOfSubTxtsW));
    int column = int(index) - row * numOfSubTxtsW;
    if (atlasTopDown != 0) {
        /* .y = top edge, .w = bottom edge so the quad's upper vertices get the
         * top of the cell (upright image) */
        return vec4(float(column) * subWidth, 1.0 - float(row) * subHeight,
                    float(column + 1) * subWidth, 1.0 - float(row + 1) * subHeight);
    }
    return vec4(float(column) * subWidth, float(row) * subHeight,
                float(column + 1) * subWidth, float(row + 1) * subHeight);
}

void main(){
    mat4 PVM = projectionMatrix * viewModelMatrixOut[0];
    vec4 ul = PVM * vec4(vec3(-0.5, 0.5, 0), 1.0);
    vec4 ur = PVM * vec4(vec3(0.5, 0.5, 0), 1.0);
    vec4 ll = PVM * vec4(vec3(-0.5, -0.5, 0), 1.0);
    vec4 lr = PVM * vec4(vec3(0.5, -0.5, 0), 1.0);

    aliveFor = (outAliveForInMs[0] / liveForInMs);
    float normalizedAliveTime = (outAliveForInMs[0] / liveForInMs) * (totalNumOfSubTxts - 1);

    vec4 currentTextCoords, prevTxtCoords;
    float txtIndex;
    blendFactor = modf (normalizedAliveTime, txtIndex);
    int rowOffset, columnOffset;

    currentTextCoords = cellCoords(txtIndex);

    if (txtIndex == totalNumOfSubTxts - 1) {
        prevTxtCoords = currentTextCoords;
    } else {
        prevTxtCoords = cellCoords(txtIndex + 1.0);
    }

    gl_Position = ul;
    textCoords = vec2(currentTextCoords.x, currentTextCoords.y);
    previousTextCoords = vec2(prevTxtCoords.x, prevTxtCoords.y);
    EmitVertex();
    gl_Position = ll;
    textCoords = vec2(currentTextCoords.x, currentTextCoords.w);
    previousTextCoords = vec2(prevTxtCoords.x, prevTxtCoords.w);
    EmitVertex();
    gl_Position = ur;
    textCoords = vec2(currentTextCoords.z, currentTextCoords.y);
    previousTextCoords = vec2(prevTxtCoords.z, prevTxtCoords.y);
    EmitVertex();
    EndPrimitive();

    gl_Position = ur;
    textCoords = vec2(currentTextCoords.z, currentTextCoords.y);
    previousTextCoords = vec2(prevTxtCoords.z, prevTxtCoords.y);
    EmitVertex();
    gl_Position = ll;
    textCoords = vec2(currentTextCoords.x, currentTextCoords.w);
    previousTextCoords = vec2(prevTxtCoords.x, prevTxtCoords.w);
    EmitVertex();
    gl_Position = lr;
    textCoords = vec2(currentTextCoords.z, currentTextCoords.w);
    previousTextCoords = vec2(prevTxtCoords.z, prevTxtCoords.w);
    EmitVertex();
    EndPrimitive();
}
