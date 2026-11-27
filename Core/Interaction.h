#ifndef INTERACTION_H
#define INTERACTION_H

#include <set>
#include <string>
#include <vector>

#include "glm/glm.hpp"
#include "rapidjson/document.h"

class Camera;
class DrivenComponent;
class Entity;
class Input;
class PressableComponent;
class Timeline;
class btRigidBody;

/*
 * What the player does to the world, as data: things to press (PressableComponent), locks that
 * open with a code, places that do something when the player walks in, the ways into a level,
 * and the things a timeline moves (DrivenComponent). A map's "Interaction" block:
 *
 *   "Interaction": {
 *     "locks": [{"name": "ward_keypad", "code": "0314", "clear": "*", "enter": "#",
 *                "onSolved": [actions], "onWrong": [actions]}],
 *     "triggers": [{"box": [[x0, y0, z0], [x1, y1, z1]], "after": "ward_door_unlocked", "once": true,
 *                   "on": [actions]}],
 *     "entries": {"from_ward": {"position": [x, y, z], "yaw": 0, "pitch": 0, "fadeIn": 1.2,
 *                               "event": "arrived_from_ward", "on": [actions]}}
 *   }
 *
 * Actions (a press's "on", a lock's, a trigger's), each one of:
 *   {"sound": "keypad_beep", "volume": 0.5, "pitch": 1, "at": [x, y, z], "radius": 300}
 *        res/sounds/<sound>.wav; at a place ("at"; a pressed thing's own place if not given), else 2D
 *   {"lock": "ward_keypad", "key": "0"}     a key into a lock: its digits gather; when as many as
 *        the code has are in (or "enter" is pressed) it opens or refuses; "clear" starts again
 *   {"event": "ward_door_unlocked"}          the timeline's sequences triggered by it start, and
 *        the triggers waiting for it ("after") are armed
 *   {"loadLevel": "Game/recursion/02_corridor_subject4.json", "entry": "from_ward", "fade": 0.9}
 *        the picture and sound fade out, the player held still, then that level, in by that entry
 *
 * A trigger's box is in the world; the player's eye inside it sets it off. An entry's "on" runs as
 * the player comes in (the door shutting behind); its fade-in is the picture's only (what is heard
 * on coming in is heard at once).
 */
class Interaction
{
    public:
        /* the map's "Interaction" block (if any), and its entities' pressables and driven things */
        void load(const rapidjson::Value &map, const std::vector<Entity *> &entities);
        /* the block as loaded (saved with the map) */
        const std::string &json() const { return m_json; }

        /* where a map lets the player in by `name` (its "entries"): false if it has no such entry */
        static bool entry(const rapidjson::Value &map, const std::string &name, glm::vec3 &position,
                          float &yaw, float &pitch);
        /* the player came in by that entry: its fade-in, its event */
        void arrive(const rapidjson::Value &map, const std::string &name, Timeline &timeline);

        /* once a tick: a left click (the mouse held for looking round: playing) presses what is aimed at */
        void input(Input &input, Timeline &timeline, float time);
        /* once a frame, after the timeline has advanced: what is aimed at, the triggers, the presses
         * going in and out, the driven things; playerBody: not in the way of the aim */
        void frame(Camera *camera, Timeline &timeline, float time, btRigidBody *playerBody);

        /* for the aim's ring: 0 nothing, 1 aimed at something within reach (eased) */
        float aim() const { return m_aimShown; }
        /* 1 just pressed, to 0 (a press's pulse) */
        float pressPulse(float time) const;
        /* black over the picture: 0..1 (leaving, or coming in); < 0: not fading */
        float fade() const { return m_fade; }
        /* a level to go to now (faded out): its path and entry */
        bool levelDue(std::string &path, std::string &entry) const;
        /* the player held still (leaving a level) */
        static bool holdsPlayer() { return s_holding; }

    private:
        struct Lock {
            std::string name, code, clear = "*", enter = "#", typed;
            rapidjson::Value onSolved, onWrong;
            bool solved = false;
        };
        struct Trigger {
            glm::vec3 low, high;
            std::string after;
            bool once = true, done = false;
            rapidjson::Value on;
        };

        void run(const rapidjson::Value &actions, Entity *source, Timeline &timeline, float time);
        void key(const std::string &lockName, const std::string &key, Timeline &timeline, float time);
        void signal(const std::string &event, Timeline &timeline);

        rapidjson::Document m_document;        /* (owns the actions' JSON) */
        std::string m_json;
        std::vector<Lock> m_locks;
        std::vector<Trigger> m_triggers;
        std::vector<PressableComponent *> m_pressables;
        std::vector<DrivenComponent *> m_driven;
        std::set<std::string> m_events;        /* what has happened (for "after") */

        PressableComponent *m_aimed = nullptr;  /* within reach, now */
        float m_aimTarget = 0.0f, m_aimShown = 0.0f, m_lastFrame = -1.0f, m_pressedAt = -100.0f;

        bool m_fadeEnded = false;
        float m_fade = -1.0f, m_fadeFrom = 0.0f, m_fadeTo = 0.0f, m_fadeStart = 0.0f, m_fadeLength = 1.0f;
        std::string m_levelPath, m_levelEntry;
        static bool s_holding;
};

#endif // INTERACTION_H
