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
        void update (long delta, const glm::mat4 &viewMatrix) override;
        bool isAlive () override;
        virtual ~SmokeParticle();

        static Texture &getTexture();
    protected:
    private:
        const unsigned int liveForInS;
};

#endif // SMOKEPARTICLE_H
