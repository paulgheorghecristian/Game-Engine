#include "SmokeParticle.h"
#include "Texture.h"

SmokeParticle::SmokeParticle(const glm::vec3 &position,
                             const glm::vec3 &velocity,
                             const glm::vec3 &scale) : IParticle (position, velocity, scale), liveForInS (10)
{

}

void SmokeParticle::update (long delta, const glm::mat4 &viewMatrix) {
    float deltaF;

    aliveForInMs += delta;
    deltaF = (float) delta / 1000;

    instaPosition = instaPosition + instaVelocity*deltaF + acceleration*(deltaF*deltaF/2.0f);
    instaVelocity = instaVelocity + acceleration*deltaF;

    generateViewModelMatrix(viewMatrix);
}

bool SmokeParticle::isAlive () {
    return aliveForInMs < liveForInS*1000 && instaPosition.y > 0 && instaPosition.y < 2000;
}

Texture &SmokeParticle::getTexture() {
    static Texture *smokeTexture = new Texture ("res/textures/particle.bmp", 0);
    return *smokeTexture;
}

SmokeParticle::~SmokeParticle()
{
    //dtor
}
