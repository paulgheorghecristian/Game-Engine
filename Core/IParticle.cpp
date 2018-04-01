#include "IParticle.h"

#include <GL/glew.h>
#include "Camera.h"
#include "Shader.h"
#include "Texture.h"

IParticle::IParticle(const glm::vec3 &position,
                     const glm::vec3 &velocity,
                     const glm::vec3 &scale,
                     const glm::vec3 &acceleration) : initialPosition (position),
                                                       initialVelocity (velocity),
                                                       instaPosition (position),
                                                       instaVelocity (velocity),
                                                       acceleration (acceleration),
                                                       instaScale (scale),
                                                       initialScale (scale),
                                                       viewModelMatrix (1.0),
                                                       aliveForInMs (0)
{

}

void IParticle::draw (Shader &shader) {
    assert (false);
    /* unused, particles are drawn using instanced rendering */
    bool result = shader.updateUniform ("viewModelMatrix", (void *) &viewModelMatrix);
    assert (result);
    glDrawArrays(GL_POINTS, 0, 1);
}

void IParticle::reset() {
    instaPosition = initialPosition;
    instaVelocity = initialVelocity;
    instaScale = initialScale;
    aliveForInMs = 0;
}

const glm::mat4 &IParticle::getViewModelMatrix() {
    return viewModelMatrix;
}

void IParticle::generateViewModelMatrix(const glm::mat4 &viewMatrix) {
    assert (false); /* UNUSED */
    glm::mat4 modelMatrix(1.0);

    /*for(int i = 0; i < 3; i++){
        for(int j = 0; j < 3; j++){
            modelMatrix[i][j] = viewMatrix[j][i];
        }
    }*/

    modelMatrix[3][0] = instaPosition.x;
    modelMatrix[3][1] = instaPosition.y;
    modelMatrix[3][2] = instaPosition.z;

    //modelMatrix = glm::scale (modelMatrix, glm::vec3(scale));
    this->viewModelMatrix = modelMatrix;
}

const glm::vec3 &IParticle::getScale () {
    return instaScale;
}

const glm::vec3 &IParticle::getInstaPosition () {
    return instaPosition;
}

const unsigned int &IParticle::getAliveForInMs () {
    return aliveForInMs;
}

IParticle::~IParticle()
{
    //dtor
}
