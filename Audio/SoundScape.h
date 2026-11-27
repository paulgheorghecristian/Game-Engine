#ifndef SOUND_SCAPE_H
#define SOUND_SCAPE_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "rapidjson/document.h"

class Camera;

/*
 * A map's own sounds: its "Audio" list, each one a sound of res/sounds, playing from the start:
 *   "Audio": [{"sound": "crickets_loop", "volume": 0.25, "follows": "night"},
 *             {"sound": "fire_loop", "position": [x, y, z], "radius": 480}, ...]
 *   "sound"     the file in res/sounds (without .wav)
 *   "volume"    1 if not given
 *   "pitch"     1 if not given
 *   "loop"      true if not given (false: once, at the start)
 *   "position"  where it comes from (3D: louder closer, silent beyond "radius", 300 if not given);
 *               none: heard the same everywhere
 *   "follows"   its volume follows something of the map: "wind" (the map's wind: louder in a
 *               gale, still a little in calm air) or "night" (silent by day). None: steady
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
        enum Follows { STEADY, WIND, NIGHT };
        struct Source {
            std::string sound;
            bool placed = false;           /* has a position: 3D */
            glm::vec3 position = glm::vec3(0.0f);
            float volume = 1.0f, radius = 300.0f, pitch = 1.0f;
            bool loop = true;
            Follows follows = STEADY;
            int voice = 0;
        };
        float loudness(const Source &source) const;
        std::vector<Source> m_sources;
        bool m_started = false;

};

#endif // SOUND_SCAPE_H
