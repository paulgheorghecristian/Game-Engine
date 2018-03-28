#ifndef IPARTICLE_H
#define IPARTICLE_H

#include <glm/glm.hpp>

class Shader;
class Camera;
class Texture;

enum ParticleFlags {
    ADDITIVE_BLENDING = 1,
    ALPHA_BLENDING = 2,
    TEXTURE_ATLAS = 4,
};

class IParticle
{
    public:
        IParticle(const glm::vec3 &position = glm::vec3 (0),
                  const glm::vec3 &velocity = glm::vec3 (0),
                  const glm::vec3 &scale = glm::vec3 (1.0f),
                  /* TODO find a way to avoid hardcoding of the gravity */
                  const glm::vec3 &acceleration = glm::vec3 (0, -10.0f, 0));
        virtual void draw (Shader &shader);
        virtual void update (double delta, Camera &camera) = 0;
        virtual bool isAlive () = 0;
        virtual void reset();
        const glm::mat4 &getViewModelMatrix ();
        const glm::vec3 &getScale ();
        const glm::vec3 &getInstaPosition ();
        const unsigned int &getAliveForInMs ();
        virtual ~IParticle();

        IParticle (const IParticle& particle) = delete;
        IParticle &operator= (const IParticle &particle) = delete;

        IParticle (IParticle &&particle) = default;
        IParticle &operator= (IParticle &&particle) = default;
    protected:
        glm::vec3 initialPosition, initialVelocity;
        glm::vec3 instaPosition, instaVelocity, acceleration;
        glm::vec3 initialScale, instaScale;
        glm::mat4 viewModelMatrix;
        unsigned int aliveForInMs;
        char flags;

        void generateViewModelMatrix(const glm::mat4 &viewMatrix);
    private:
};

#endif // IPARTICLE_H
