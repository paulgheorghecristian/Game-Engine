#include "ParticleEmitterComponent.h"

#include "Entity.h"
#include "ParticleEmitter.h"
#include "RenderingMaster.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

ParticleEmitterComponent::ParticleEmitterComponent(const rapidjson::Value &json) : m_offset(0.0f)
{
    if (json.HasMember("offset") && json["offset"].IsArray() && json["offset"].Size() >= 3) {
        m_offset = glm::vec3(json["offset"][0].GetFloat(), json["offset"][1].GetFloat(), json["offset"][2].GetFloat());
    }
    m_emitter = new ParticleEmitter(json);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    m_json = buffer.GetString();
}

void ParticleEmitterComponent::init()
{
    RenderingMaster::getInstance()->addParticleEmitter(m_emitter);
    update();
    m_emitter->restart(); /* emit from the entity, not from where the emitter pre-warmed */
}

void ParticleEmitterComponent::input(Input &inputManager)
{
}

void ParticleEmitterComponent::update()
{
    const glm::mat4 &model = _entity->getTransform().getModelMatrix();
    glm::mat3 rotation = glm::mat3(glm::normalize(glm::vec3(model[0])),
                                   glm::normalize(glm::vec3(model[1])),
                                   glm::normalize(glm::vec3(model[2])));
    m_emitter->setOrigin(glm::vec3(model * glm::vec4(m_offset, 1.0f)), rotation);
}

void ParticleEmitterComponent::render()
{
}

const unsigned int ParticleEmitterComponent::getFlag() const
{
    return Entity::Flags::PARTICLE_EMITTER;
}

std::string ParticleEmitterComponent::jsonify()
{
    return "\"ParticleEmitterComponent\":" + m_json;
}

ParticleEmitterComponent::~ParticleEmitterComponent()
{
    RenderingMaster::getInstance()->removeParticleEmitter(m_emitter);
    delete m_emitter;
}
