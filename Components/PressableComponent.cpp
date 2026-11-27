#include "PressableComponent.h"

#include <limits>

#include "RenderComponent.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

static glm::vec3 readVec3(const rapidjson::Value &json, const char *key, glm::vec3 fallback)
{
    if (!json.HasMember(key) || !json[key].IsArray() || json[key].Size() != 3) return fallback;
    const auto &a = json[key];
    return glm::vec3(a[0].GetFloat(), a[1].GetFloat(), a[2].GetFloat());
}

PressableComponent::PressableComponent(const rapidjson::Value &json)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);
    m_json = buffer.GetString();

    if (json.HasMember("reach") && json["reach"].IsNumber()) reach = json["reach"].GetFloat();
    if (json.HasMember("pressTime") && json["pressTime"].IsNumber()) pressTime = json["pressTime"].GetFloat();
    travel = readVec3(json, "travel", travel);
    m_document.SetObject();
    if (json.HasMember("on") && json["on"].IsArray()) {
        m_actions.CopyFrom(json["on"], m_document.GetAllocator());
    } else {
        m_actions.SetArray();
    }
}

bool PressableComponent::localBounds(glm::vec3 &low, glm::vec3 &high)
{
    if (!m_boundsKnown) {
        m_boundsKnown = true;
        RenderComponent *render = _entity != nullptr ?
            (RenderComponent *) _entity->getComponent(Entity::Flags::RENDERABLE) : nullptr;
        if (render != nullptr) {
            glm::vec3 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max());
            for (Mesh *mesh : render->getRenderingObject().getMeshes()) {
                for (const Vertex &v : mesh->getVertices()) {
                    lo = glm::min(lo, v.positionCoords);
                    hi = glm::max(hi, v.positionCoords);
                    m_hasBounds = true;
                }
            }
            m_low = lo;
            m_high = hi;
        }
    }
    low = m_low;
    high = m_high;
    return m_hasBounds;
}
