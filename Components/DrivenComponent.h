#ifndef DRIVENCOMPONENT_H
#define DRIVENCOMPONENT_H

#include <string>

#include "Component.h"
#include "Entity.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "rapidjson/document.h"

/*
 * An entity moved by a timeline parameter: a door swinging open, a hatch sliding, a lever going
 * down. At the parameter's value v it is turned by v * "rotate" (degrees round x, y and z, about
 * "pivot") and moved by v * "move", from where the map puts it (v = 0). A physics body on it moves
 * with it (it blocks, then lets through). "anchor": the timeline's sounds can be placed at its
 * pivot by that name (a creak where the hinges are).
 *
 *   "Driven": {"param": "ward_door", "pivot": [-11.78, 0, 88.54], "rotate": [0, 90, 0],
 *              "move": [0, 0, 0], "anchor": "ward_door"}
 *
 * Core/Interaction applies it each frame.
 */
class DrivenComponent : public Component
{
    public:
        explicit DrivenComponent(const rapidjson::Value &json);
        void input(Input &) override {}
        void update() override {}
        void render() override {}
        const unsigned int getFlag() const override { return Entity::Flags::DRIVEN; }
        std::string jsonify() override { return "\"Driven\":" + m_json; }

        /* puts it where the value v takes it */
        void apply(float v);

        std::string param, anchor;
        glm::vec3 pivot = glm::vec3(0.0f), rotate = glm::vec3(0.0f), move = glm::vec3(0.0f);

    private:
        std::string m_json;
        bool m_hasRest = false, m_bodyMadeKinematic = false;
        float m_applied = -1.0f;
        glm::vec3 m_restPosition = glm::vec3(0.0f);
        glm::quat m_restRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

#endif // DRIVENCOMPONENT_H
