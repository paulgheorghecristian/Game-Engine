#ifndef TIMELINE_H
#define TIMELINE_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "rapidjson/document.h"

/*
 * Scripted sequences, as data. A map's "Sequences" array (or, for a map with
 * a Moon destruction and no sequences of its own, the default
 * Game/sequences/kugelblitz_storm.json). An entry can be a sequence, or
 * {"file": "Game/sequences/....json"} for one kept in its own file (what
 * tools/timeline_editor.py edits):
 *
 *   {"name": "the storm", "trigger": "kugelblitzFormed" | "mapStart",
 *    "params": {"storm": [[t, v], ...],                        keys, smooth
 *               "dust": {"keys": [[t, v], ...], "curve": "linear" | "smooth" | "step"},
 *               "gain": 1.8},                                  a constant
 *    (a key can carry its own bezier handles, as an animation editor shapes
 *    them: [t, v, leftX, leftY, rightX, rightY], x along the time to the next
 *    / previous key, y along the value's change, both 0..1; or [t, v, "hold"]
 *    to stay at v until the next key)
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
        /* the same, in place of what is loaded, while it plays: the clocks
         * and the sounds already playing carry on (live editing) */
        void replace(const rapidjson::Value &sequences);
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

        /* the scene's clock: seconds since the start (the map loaded, its animation starting).
         * Sequences triggered by "mapStart" run on it; others start on it later */
        float clock() const { return m_clock; }
        /* put the clock at t (the timeline editor); 0 stops it, 1 runs it */
        void seek(float t) { m_clock = t; }
        void setTimeScale(float scale) { m_scale = scale; }

        /* for whoever shows or edits it (the timeline editor, through the engine's state), all on
         * the scene's clock: where each sequence's own 0 is (in the order loaded) */
        std::vector<float> starts() const;
        /* a parameter at scene time T (fallback where no started sequence sets it) */
        float valueAtScene(const std::string &name, float T, float fallback = 0.0f) const;
        /* how many seconds there is to see: to the end of the longest sequence (what a sequence
         * gives as "length", else a while past its last key, sound or event) */
        float sceneLength() const;
        /* when the first sound starts (-1: none) */
        float firstSoundScene() const;
        /* the first scene time a parameter of the sequences that start at 0 reaches `level`
         * (-1: it never does, or none sets it). E.g. "collapse" 1: the black hole has formed */
        float firstReach(const std::string &name, float level) const;
        /* what a timeline's sound can be given, as JSON (the ranges an editor offers) */
        static std::string soundsJson();
        void setAnchor(const std::string &name, const glm::vec3 &position) { m_anchors[name] = position; }

        /* parts of it isolated (the timeline editor's "solo"): only the soloed parameters and sounds
         * play, each given as its sequence (in the order loaded) and its name / its index among the
         * sequence's sounds. Every other parameter named in `off` reads as that value (switched off),
         * whether a sequence sets it or not; one not in `off` stays as the timeline has it. The other
         * sounds are not heard. Nothing soloed: all of it plays. (Kept when the timeline is replaced) */
        void setSolo(const std::set<std::pair<int, std::string>> &params, const std::set<std::pair<int, int>> &sounds,
                     const std::map<std::string, float> &off);
        bool soloing() const { return !m_soloParams.empty() || !m_soloSounds.empty(); }

    private:
        struct Key {
            float t, v;
            float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f;   /* bezier handles, if it has its own */
            bool handles = false, hold = false;
        };
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
            float length = -1.0f;          /* "length": seconds of it to show (-1: not given) */
        };
        static Track parseTrack(const rapidjson::Value &value);
        float startOf(const Sequence &sequence) const;
        void loadOne(const rapidjson::Value &sequence);
        void stopSounds(Sequence &sequence);

        std::vector<Sequence> m_sequences;
        std::map<std::string, float> m_params;
        std::map<std::string, glm::vec3> m_anchors;
        std::vector<std::string> m_fired;
        bool m_running = false;
        float m_time = -1.0f;
        float m_clock = 0.0f, m_lastRender = -1.0f, m_scale = 1.0f;
        mutable std::map<std::string, float> m_reached;     /* firstReach's answers (cleared on load) */
        std::set<std::pair<int, std::string>> m_soloParams;
        std::set<std::pair<int, int>> m_soloSounds;
        std::map<std::string, float> m_soloOff;
};

#endif // TIMELINE_H
