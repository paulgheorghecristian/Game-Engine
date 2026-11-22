#ifndef PARTICLE_EMITTER_COMPONENT_H
#define PARTICLE_EMITTER_COMPONENT_H

#include <string>
#include <glm/glm.hpp>

#include "Component.h"
#include "rapidjson/document.h"

class ParticleEmitter;

/*
 * Attaches a ParticleEmitter to an entity: the emitter follows the entity's
 * position and rotation every tick (particles already emitted stay where they
 * are, so moving emitters leave trails).
 *
 * JSON: "ParticleEmitterComponent": { <any "ParticleEmitters" keys>,
 *                                     "offset": [x, y, z] }   (entity local space)
 */
class ParticleEmitterComponent : public Component
{
    public:
        explicit ParticleEmitterComponent(const rapidjson::Value &json);
        void input(Input &inputManager) override;
        void update() override;
        void render() override;
        void init() override;
        const unsigned int getFlag() const override;

        std::string jsonify() override;

        virtual ~ParticleEmitterComponent();

    private:
        ParticleEmitter *m_emitter;
        glm::vec3 m_offset;
        std::string m_json;     /* for jsonify() (saving the map) */
};

#endif // PARTICLE_EMITTER_COMPONENT_H
