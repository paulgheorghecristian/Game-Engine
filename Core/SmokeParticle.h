#ifndef SMOKEPARTICLE_H
#define SMOKEPARTICLE_H

#include "IParticle.h"

class Texture;

/* TODO make move constr */
class SmokeParticle : public IParticle
{
    public:
        SmokeParticle(const glm::vec3 &position,
                      const glm::vec3 &velocity,
                      const glm::vec3 &scale);
        void update (long delta, Camera &camera) override;
        bool isAlive () override;
        virtual ~SmokeParticle();

        SmokeParticle (const SmokeParticle& particle) = delete;
        SmokeParticle &operator= (const SmokeParticle &particle) = delete;

        SmokeParticle (SmokeParticle &&particle) = default;
        SmokeParticle &operator= (SmokeParticle &&particle) = default;

        float getDistanceToCamera ();

        static Texture &getTexture();
        static const unsigned int &getLiveForInMs ();
    protected:
    private:
        static const unsigned int liveForInS, liveForInMs;
        float distanceToCamera;
};

#endif // SMOKEPARTICLE_H
