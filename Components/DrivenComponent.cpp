#include "DrivenComponent.h"

#include "PhysicsComponent.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

static glm::vec3 readVec3(const rapidjson::Value &json, const char *key, glm::vec3 fallback)
{
    if (!json.HasMember(key) || !json[key].IsArray() || json[key].Size() != 3) return fallback;
    const auto &a = json[key];
    return glm::vec3(a[0].GetFloat(), a[1].GetFloat(), a[2].GetFloat());
}

DrivenComponent::DrivenComponent(const rapidjson::Value &json)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    m_json = buffer.GetString();

    if (json.HasMember("param") && json["param"].IsString()) param = json["param"].GetString();
    if (json.HasMember("anchor") && json["anchor"].IsString()) anchor = json["anchor"].GetString();
    pivot = readVec3(json, "pivot", pivot);
    rotate = readVec3(json, "rotate", rotate);
    move = readVec3(json, "move", move);
}

void DrivenComponent::apply(float v)
{
    if (_entity == nullptr) return;
    Transform &transform = _entity->getTransform();
    if (!m_hasRest) {
        m_hasRest = true;
        m_restPosition = transform.getPosition();
        m_restRotation = transform.getRotation();
    }
    if (std::abs(v - m_applied) < 1e-6f) return;        /* (not moved since) */
    m_applied = v;

    glm::vec3 angles = glm::radians(rotate * v);
    glm::quat turn = glm::angleAxis(angles.z, glm::vec3(0, 0, 1)) * glm::angleAxis(angles.y, glm::vec3(0, 1, 0)) *
                     glm::angleAxis(angles.x, glm::vec3(1, 0, 0));
    glm::vec3 position = pivot + turn * (m_restPosition - pivot) + move * v;
    glm::quat rotation = turn * m_restRotation;
    transform.setPosition(position);
    transform.setRotation(rotation);

    /* its body goes with it: a body moved by hand is a kinematic one (the physics reads where it is
     * each step, and what it touches is pushed, not passed through) */
    PhysicsComponent *physics = (PhysicsComponent *) _entity->getComponent(Entity::Flags::DYNAMIC);
    if (physics != nullptr && physics->getRigidBody() != nullptr) {
        btRigidBody *body = physics->getRigidBody();
        if (!m_bodyMadeKinematic) {
            m_bodyMadeKinematic = true;
            body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            body->setActivationState(DISABLE_DEACTIVATION);
        }
        btTransform t;
        t.setIdentity();
        t.setOrigin(btVector3(position.x, position.y, position.z));
        t.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
        body->setWorldTransform(t);
        if (body->getMotionState() != nullptr) body->getMotionState()->setWorldTransform(t);
    }
}
