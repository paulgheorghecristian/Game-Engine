#include "SmokeParticle.h"
#include "Texture.h"
#include "Camera.h"

#include <glm/gtx/norm.hpp>

const float SmokeParticle::liveForInS = 1.3;
const unsigned int SmokeParticle::liveForInMs = liveForInS * 1000;

SmokeParticle::SmokeParticle(const glm::vec3 &position,
                             const glm::vec3 &velocity,
                             const glm::vec3 &scale) : IParticle (position, velocity, scale)
{

}

void SmokeParticle::update (double delta, Camera &camera) {
    float deltaF;
    /* TODO make this as accurate as possible for smoke */

    if (msDelay <= 0) {
        simulate = true;
    }

    if (!simulate) {
        msDelay -= delta;
        return;
    }

    if (isAlive ()) {
        aliveForInMs += delta;
        deltaF = (float) delta / 1000;

        instaScale.x += 150 * deltaF;

        instaPosition = instaPosition + instaVelocity*deltaF + (glm::vec3(0, acceleration.y+instaScale.x*25, 0))*(deltaF*deltaF/2.0f);
        instaVelocity = instaVelocity + acceleration*deltaF;
    } else {
        reset ();
        simulate = false;
        msDelay = msDelayCopy;
    }

    distanceToCamera = -glm::distance2 (camera.getPosition(), instaPosition);
    //generateViewModelMatrix(viewMatrix);
}

bool SmokeParticle::isAlive () {
    return aliveForInMs < liveForInMs;
}

Texture &SmokeParticle::getTexture() {
    static Texture smokeTexture ("res/textures/smoke.png", 0, 8, 5);
    return smokeTexture;
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
