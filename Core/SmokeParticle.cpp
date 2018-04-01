#include "SmokeParticle.h"
#include "Texture.h"
#include "Camera.h"

#include <glm/gtx/norm.hpp>

unsigned int SmokeParticle::liveForInMs;

SmokeParticle::SmokeParticle(const glm::vec3 &position,
                             const glm::vec3 &velocity,
                             const glm::vec3 &scale,
                             float diffusionFactor,
                             float buoyancyFactor) : IParticle (position, velocity, scale),
                                                    diffusionFactor (diffusionFactor),
                                                    buoyancyFactor (buoyancyFactor)
{
    simulate = false;
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

        instaScale.x += diffusionFactor * deltaF;

        instaPosition = instaPosition + instaVelocity*deltaF + (glm::vec3(0, acceleration.y+instaScale.x*buoyancyFactor, 0))*(deltaF*deltaF/2.0f);
        instaVelocity = instaVelocity + acceleration*deltaF;
    } else {
        reset ();
        simulate = false;
        msDelay = msDelayCopy;
    }

    distanceToCamera = -glm::distance2 (camera.getPosition(), instaPosition);
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
