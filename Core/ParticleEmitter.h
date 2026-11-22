#ifndef PARTICLE_EMITTER_H
#define PARTICLE_EMITTER_H

#include <vector>
#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "rapidjson/document.h"

class Camera;
class Shader;
class Texture;

/*
 * Data-driven particle emitter, configured from a map's "ParticleEmitters"
 * array. Behaviours:
 *
 *  "smoke"  the engine's smoke model (SmokeParticle): rises with buoyancy
 *           proportional to its size, diffuses (grows) over time
 *  "spray"  ballistic particles launched in a cone with gravity (the World2
 *           particle system: fountains, sparks, embers)
 *  "drift"  slow wandering particles kept inside a volume (fireflies, motes)
 *  "glow"   static, never-fading billboards (halos around lights in haze);
 *           "positions": [[x,y,z], ...] places one per entry
 *
 * All emitters share particleRenderShader (instanced point -> billboard quad,
 * texture-atlas animation, soft particles) and are drawn into the half
 * resolution particle buffer that is composited in the buffer-combination pass.
 *
 * JSON keys (all optional except "position"):
 *   type, texture, atlas [cols, rows], atlasTopDown, count, lifetimeMs,
 *   position [x,y,z], radius, height, direction [x,y,z], spread, speed [min,max],
 *   gravity, buoyancy, startScale, growth, scaleJitter, tint [r,g,b],
 *   brightness, blend ("alpha"|"additive"), fadeIn, wander
 */
class ParticleEmitter
{
    public:
        enum Type { SMOKE = 0, SPRAY, DRIFT, GLOW };

        explicit ParticleEmitter(const rapidjson::Value &json);
        ~ParticleEmitter();

        ParticleEmitter(const ParticleEmitter &) = delete;
        ParticleEmitter &operator=(const ParticleEmitter &) = delete;

        void update(Camera &camera, float dtMs);
        /* expects the particle framebuffer to be bound and the scene depth
         * texture bound on unit 3 (soft particles) */
        void draw(const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix,
                  int renderWidth, int renderHeight, float alpha = 1.0f);

        static Shader &getShader();

        /* move the emitter (ParticleEmitterComponent): new particles spawn
         * around `position`, launched along the rotated "direction" */
        void setOrigin(const glm::vec3 &position, const glm::mat3 &rotation);
        /* forget all particles; they are re-emitted (staggered) from the current origin */
        void restart();

    private:
        struct Particle {
            glm::vec3 position, velocity;
            glm::vec3 previousPosition; /* at the previous tick, for interpolation */
            float scale;
            float ageMs, lifeMs;
            float distanceToCamera;
        };

        Type m_type;
        std::vector<Particle> m_particles;

        glm::vec3 m_position, m_direction, m_tint;
        glm::vec3 m_localDirection;
        std::vector<glm::vec3> m_glowOffsets; /* glow positions relative to m_position */
        float m_radius, m_height, m_spread;
        float m_speedMin, m_speedMax;
        float m_gravity, m_buoyancy;
        float m_startScale, m_growth, m_scaleJitter;
        float m_lifetimeMs, m_brightness, m_fadeIn, m_wander;
        bool m_additive, m_atlasTopDown;

        Texture *m_texture;
        GLuint m_vao, m_vbo;
        std::vector<float> m_instanceData;

        unsigned int m_rngState;
        float rand01();
        glm::vec3 randomInUnitSphere();

        void respawn(Particle &p);
        void simulate(Particle &p, float dtMs);
};

#endif // PARTICLE_EMITTER_H
