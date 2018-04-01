#ifndef SMOKEPARTICLE_H
#define SMOKEPARTICLE_H

#include "IParticle.h"

class Texture;

class SmokeParticle : public IParticle
{
    public:
        SmokeParticle(const glm::vec3 &position,
                      const glm::vec3 &velocity,
                      const glm::vec3 &scale,
                      float diffusionFactor,
                      float buoyancyFactor);
        void update (double delta, Camera &camera) override;
        bool isAlive () override;
        virtual ~SmokeParticle();

        SmokeParticle (const SmokeParticle& particle) = delete;
        SmokeParticle &operator= (const SmokeParticle &particle) = delete;

        SmokeParticle (SmokeParticle &&particle) = default;
        SmokeParticle &operator= (SmokeParticle &&particle) = default;

        float getDistanceToCamera ();
        float msDelay, msDelayCopy; /* TODO maybe i want this to be in IParticle? */
        static unsigned int liveForInMs; /* maybe i don't want this shared ? */

        static Texture &getTexture();
        static const unsigned int &getLiveForInMs ();
    protected:
    private:
        float distanceToCamera, diffusionFactor, buoyancyFactor;
        bool simulate;
};

#endif // SMOKEPARTICLE_H
