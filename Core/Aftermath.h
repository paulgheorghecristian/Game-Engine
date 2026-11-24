#ifndef AFTERMATH_H
#define AFTERMATH_H

#include <map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Camera;
class ParticleEmitter;
class PointLight;
class BlackHole;
class Timeline;

/*
 * The storm after the sky's Kugelblitz, as behaviours. When it happens and
 * how much of it there is comes from the map's timeline (Core/Timeline: the
 * map's "Sequences", or Game/sequences/kugelblitz_storm.json); this only
 * reads its parameters, each frame:
 *
 *   (the list of them, each with its range and what it does, is PARAMETERS
 *    in Aftermath.cpp: Aftermath::parameters(). The timeline editor shows
 *    that list, so it is the one place to add or describe one.)
 *
 * It places the timeline's sound anchors "front" (the shockwave) and
 * "upwind". The hole's "quitAfter" still fades out and closes the game.
 */
class Aftermath
{
    public:
        /* a value the storm reads from the timeline */
        struct Parameter {
            const char *name;
            const char *group;          /* how the editor groups them */
            float min, max;             /* the editor's slider (values outside still work) */
            float standard;             /* what it is when the timeline does not set it */
            const char *about;          /* what it does, in plain words */
            const char *after = nullptr;   /* its value is seconds after this moment (an id of
                                              timelineJson's moments): shown along the time axis */
        };
        static const std::vector<Parameter> &parameters();
        /* that list as JSON (for tools/timeline_editor.py) */
        static std::string parametersJson();
        /* where a timeline's sound can be placed (the anchors update() sets), as JSON */
        static std::string anchorsJson();
        /* the timeline as a whole, for whoever shows it, as JSON members, all on the scene's clock
         * (0 = the start): its length, where each sequence starts, its first sound, and its named
         * moments (each worked out from the map or the timeline, none a fixed time) */
        static std::string timelineJson(const Timeline &timeline);
        /* while parts of a timeline are isolated (the editor's solo): what every other effect is
         * then, by name (its standard value: off). Not the ones that only change another (how loud,
         * how gusty, where the front is): those stay as the timeline has them, so what is isolated
         * looks and sounds as it does in the whole */
        static std::map<std::string, float> isolatedOff();

        ~Aftermath();
        /* once a frame, after the timeline advanced and before the renderer
         * prepares it; true when the ending is over and the game should close */
        bool update(Camera *camera, Timeline &timeline);
        /* what it applied last frame, each effect by name, and the sounds it played since the
         * timeline (re)started (how many, the last one's loudness), as JSON members: for
         * tools/timeline_editor.py, to see what each effect really does */
        std::string appliedJson() const;

    private:
        void driveCollapse(BlackHole *hole, const Timeline &timeline);
        void reset();
        void startDebris();
        void placeDebris(Camera *camera, const glm::vec3 &downwind, float ground, float wall, float frontDistance,
                         float stormDust, float rain, float t);
        void updateFires(int count, float t, const glm::vec3 &eye);
        void putOutFires();
        /* its sounds, at the timeline's "gain" */
        int play(const std::string &name, float volume, float pitch = 1.0f, bool loop = false);
        int play3D(const std::string &name, const glm::vec3 &position, float volume, float radius, float pitch = 1.0f);

        bool m_active = false;
        float m_lastT = -1.0f;            /* the timeline's clock last frame (a rewind resets) */
        glm::vec2 m_stormDirection = glm::vec2(1.0f, 0.0f);   /* xz, from the hole towards the player */
        glm::vec3 m_holeDirection = glm::vec3(0.0f, 1.0f, 0.0f);
        float m_gain = 1.0f;

        std::vector<ParticleEmitter *> m_debris;
        bool m_debrisShown = false;

        /* (its clock is negative before the black hole forms: "none yet" is far below any time) */
        static constexpr float UNSCHEDULED = -1e9f;
        static constexpr int NO_BOLT = -2000000000;
        int m_gusts = 0, m_booms = 0, m_thuds = 0, m_lastBolt = NO_BOLT;
        float m_nextGust = UNSCHEDULED, m_nextBoom = UNSCHEDULED, m_nextThud = UNSCHEDULED;
        struct Delayed { float at; std::string sound; float volume, pitch; };
        std::vector<Delayed> m_delayed;   /* thunder, after its lightning */
        std::vector<int> m_voices;        /* its one-shots still sounding (cut by a jump in time) */
        int remember(int voice);
        void cutVoices();

        struct Fire { std::vector<ParticleEmitter *> emitters; PointLight *light; float litAt, size; };
        std::vector<Fire> m_fires;
        std::vector<std::pair<std::string, float>> m_applied;
        void applied(const char *name, float v);
        int m_thunders = 0;
        float m_lastGust = 0.0f, m_lastBoom = 0.0f, m_lastThud = 0.0f, m_lastThunder = 0.0f;
};

#endif // AFTERMATH_H
