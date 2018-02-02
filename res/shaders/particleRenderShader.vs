#version 330

layout(location = 0) in vec4 positionAndScale;

uniform mat4 viewMatrix;

out vec3 eyeSpaceNormal;
out mat4 viewModelMatrixOut;

void main(){
	mat4 newModelMatrix = mat4(1.0);
	mat4 scaleMatrix = mat4 (positionAndScale[3],0,0,0,
					    0,positionAndScale[3],0,0,
					    0, 0, positionAndScale[3],0,
					    0, 0, 0, 1);
	for(int i = 0; i < 3; i++){
		for(int j = 0; j < 3; j++){
			newModelMatrix [i][j] = viewMatrix[j][i];
		}
	}

	newModelMatrix[3][0] = positionAndScale[0];
	newModelMatrix[3][1] = positionAndScale[1];
	newModelMatrix[3][2] = positionAndScale[2];


    viewModelMatrixOut = viewMatrix * newModelMatrix * scaleMatrix;
    gl_Position = vec4(vec3(0), 1.0);
}
