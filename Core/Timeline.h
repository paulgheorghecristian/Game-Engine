#ifndef TIMELINE_H
#define TIMELINE_H

#include <map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "rapidjson/document.h"

/*
 * Scripted sequences, as data. A map's "Sequences" array (or, for a map with
 * a Moon destruction and no sequences of its own, the default
 * Game/sequences/kugelblitz_storm.json):
 *
 *   {"name": "the storm", "trigger": "kugelblitzFormed" | "mapStart",
 *    "params": {"storm": [[t, v], ...],                        keys, smooth
 *               "dust": {"keys": [[t, v], ...], "curve": "linear" | "smooth" | "step"},
 *               "gain": 1.8},                                  a constant
 *    "sounds": [{"at": 8, "sound": "blast", "volume": 1, "pitch": 0.75},
 *               {"at": 6.5, "sound": "storm_loop", "loop": true, "volume": [[6.5, 0], [7.2, 1.1]],
 *                "anchor": "front", "radius": 3000}],
 *    "events": [{"at": 40, "event": "quit"}]}
 *
 * A sequence's clock runs from its trigger: "kugelblitzFormed" from the
 * moment the sky's Moon destruction finishes while watched (and it rewinds
 * with Restart / Reset), "mapStart" from the start of the map. Each frame the
 * running sequences set named parameters; the systems that read them do the
 * work (Core/Aftermath: the storm's wind, dust, rain, lightning, debris,
 * fires, camera shake...). The timings and the amounts are data; the
 * behaviour is code. Sounds play at the "gain" parameter's loudness, 2D or
 * from an anchor a system places (Aftermath: "front", "upwind").
 */
class Timeline
{
    public:
        ~Timeline();
        /* a map's "Sequences" (an array), or a file of one sequence or an array */
        void load(const rapidjson::Value &sequences);
        bool loadFile(const std::string &path);
        void clear();
        bool empty() const { return m_sequences.empty(); }

        /* once a frame, first: the clocks, the parameters, the events */
        void advance(float renderTime);
        /* then, once the systems have placed their anchors: the sounds */
        void playSounds(const glm::vec3 &eye);

        /* a parameter of the running sequences (fallback when none sets it) */
        float get(const std::string &name, float fallback = 0.0f) const;
        bool running() const { return m_running; }
        /* the clock of the running sequence (seconds since its trigger; -1: none) */
        float time() const { return m_time; }
        /* an event that fired this frame */
        bool fired(const std::string &event) const;
        void setAnchor(const std::string &name, const glm::vec3 &position) { m_anchors[name] = position; }

    private:
        struct Key { float t, v; };
        struct Track {
            std::vector<Key> keys;
            int curve = 1;                 /* 0 linear, 1 smooth, 2 step */
            float evaluate(float t) const;
        };
        struct Sound {
            float at = 0.0f;
            std::string sound;
            Track volume;
            float pitch = 1.0f;
            bool loop = false;
            std::string anchor;            /* empty: 2D */
            float radius = 800.0f;
            bool started = false;
            int voice = 0;
        };
        struct Event { float at; std::string name; bool done = false; };
        struct Sequence {
            std::string name, trigger;
            std::map<std::string, Track> params;
            std::vector<Sound> sounds;
            std::vector<Event> events;
            bool running = false;
            float t = -1.0f, startedAt = 0.0f;
        };
        static Track parseTrack(const rapidjson::Value &value);
        void loadOne(const rapidjson::Value &sequence);
        void stopSounds(Sequence &sequence);

        std::vector<Sequence> m_sequences;
        std::map<std::string, float> m_params;
        std::map<std::string, glm::vec3> m_anchors;
        std::vector<std::string> m_fired;
        bool m_running = false;
        float m_time = -1.0f;
        float m_mapStart = -1.0f;
};

#endif // TIMELINE_H
