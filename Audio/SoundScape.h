#ifndef SOUND_SCAPE_H
#define SOUND_SCAPE_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "rapidjson/document.h"

class Camera;

/*
 * A map's sound: configured by its "Audio" block (all optional):
 *   "wind": volume of the wind loop (scaled by the map's wind),
 *   "crickets": volume of the crickets at night,
 *   "sources": [{"sound": "fire_loop", "position": [x, y, z],
 *                "volume": 0.8, "radius": 300}, ...]  (looping, in 3D)
 *
 * It also voices the sky's Kugelblitz: a deep, nuclear-like blast with the
 * ignition flash, then heavy distant booms and thunder as debris reaches
 * the atmosphere during the death spiral (on its playback clock; in real
 * life only the debris would be heard: sound does not cross space).
 */
class SoundScape
{
    public:
        void load(const rapidjson::Value &json);
        /* once a frame */
        void update(Camera *camera);
        void clear();

    private:
        struct Source {
            std::string sound;
            glm::vec3 position;
            float volume, radius;
            int voice = 0;
        };
        float m_wind = 0.0f, m_crickets = 0.0f;
        std::vector<Source> m_sources;
        int m_windVoice = 0, m_cricketVoice = 0;
        bool m_started = false;

        /* Kugelblitz */
        float m_lastIgnition = -1e9f;
        float m_lastCollapse = -1.0f;
        int m_booms = 0;                      /* how many of the scheduled booms have played */
        float m_blastDue = -1.0f;             /* playback-clock time of the delayed blast (-1: none) */
        float m_lastPlayTime = 0.0f;
};

#endif // SOUND_SCAPE_H
