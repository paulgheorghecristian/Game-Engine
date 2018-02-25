#include "SmokeParticle.h"
#include "Texture.h"
#include "Camera.h"

#include <glm/gtx/norm.hpp>

const unsigned int SmokeParticle::liveForInS = 5;
const unsigned int SmokeParticle::liveForInMs = liveForInS * 1000;

SmokeParticle::SmokeParticle(const glm::vec3 &position,
                             const glm::vec3 &velocity,
                             const glm::vec3 &scale) : IParticle (position, velocity, scale)
{

}

void SmokeParticle::update (long delta, Camera &camera) {
    float deltaF;

    if (isAlive ()) {
        aliveForInMs += delta;
        deltaF = (float) delta / 1000;

        instaPosition = instaPosition + instaVelocity*deltaF + acceleration*(deltaF*deltaF/2.0f);
        instaVelocity = instaVelocity + acceleration*deltaF;
    } else {
        reset ();
    }

    distanceToCamera = glm::distance2 (camera.getPosition(), instaPosition);

    //generateViewModelMatrix(viewMatrix);
}

bool SmokeParticle::isAlive () {
    return aliveForInMs < liveForInMs && instaPosition.y > 0 && instaPosition.y < 2000;
}

Texture &SmokeParticle::getTexture() {
    static Texture *smokeTexture = new Texture ("res/textures/smoke.png", 0, 8, 8);
    return *smokeTexture;
}

const unsigned int &SmokeParticle::getLiveForInMs () {
    return liveForInMs;
}

float SmokeParticle::getDistanceToCamera () {
    return distanceToCamera;
}

SmokeParticle::~SmokeParticle()
{
    //dtor
}
