#ifndef PRESSABLECOMPONENT_H
#define PRESSABLECOMPONENT_H

#include <string>

#include "Component.h"
#include "Entity.h"
#include "glm/glm.hpp"
#include "rapidjson/document.h"

/*
 * Something the player presses: aimed at (the middle of the screen) within reach, and a left click. A key of a
 * keypad, a button, a switch. What a press does is data: "on", the actions Core/Interaction runs
 * (a sound, a key into a lock, an event, a level to go to). It goes in by "travel" (world units)
 * and comes back, over "pressTime" seconds, as a key does.
 *
 *   "Pressable": {"reach": 30, "travel": [0, 0, 0.12], "pressTime": 0.14,
 *                 "on": [{"sound": "keypad_beep", "volume": 0.5}, {"lock": "ward_keypad", "key": "0"}]}
 *
 * Where it can be aimed at: its mesh's bounds (a box round it, turned with the entity).
 */
class PressableComponent : public Component
{
    public:
        explicit PressableComponent(const rapidjson::Value &json);
        void input(Input &) override {}
        void update() override {}
        void render() override {}
        const unsigned int getFlag() const override { return Entity::Flags::PRESSABLE; }
        std::string jsonify() override { return "\"Pressable\":" + m_json; }

        /* the actions of a press ("on"; an array, maybe empty) */
        const rapidjson::Value &actions() const { return m_actions; }
        /* its mesh's bounds in its own space; false: it has no mesh */
        bool localBounds(glm::vec3 &low, glm::vec3 &high);

        float reach = 30.0f;
        glm::vec3 travel = glm::vec3(0.0f);
        float pressTime = 0.14f;

        /* the press going in and out (Core/Interaction): when it was pressed, where it rests */
        float pressedAt = -1.0f;
        bool hasRest = false;
        glm::vec3 rest = glm::vec3(0.0f);

    private:
        std::string m_json;
        rapidjson::Document m_document;
        rapidjson::Value m_actions;
        bool m_boundsKnown = false, m_hasBounds = false;
        glm::vec3 m_low = glm::vec3(0.0f), m_high = glm::vec3(0.0f);
};

#endif // PRESSABLECOMPONENT_H
