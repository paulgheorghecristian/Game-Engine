#ifndef SMOKEPARTICLE_H
#define SMOKEPARTICLE_H

#include "IParticle.h"

class Texture;

class SmokeParticle : public IParticle
{
    public:
        SmokeParticle(const glm::vec3 &position,
                      const glm::vec3 &velocity,
                      const glm::vec3 &scale);
        void update (double delta, Camera &camera) override;
        bool isAlive () override;
        virtual ~SmokeParticle();

        SmokeParticle (const SmokeParticle& particle) = delete;
        SmokeParticle &operator= (const SmokeParticle &particle) = delete;

        SmokeParticle (SmokeParticle &&particle) = default;
        SmokeParticle &operator= (SmokeParticle &&particle) = default;

        float getDistanceToCamera ();
        bool simulate;
        float msDelay, msDelayCopy;

        static Texture &getTexture();
        static const unsigned int &getLiveForInMs ();
    protected:
    private:
        static const unsigned int liveForInMs;
        static const float liveForInS;
        float distanceToCamera;
};

#endif // SMOKEPARTICLE_H
