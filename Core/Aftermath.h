#ifndef AFTERMATH_H
#define AFTERMATH_H

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
 *   storm, calm, windTurn, gustiness   the gale (wind, fog, clouds, waves) and the calm before it
 *   shake                              the camera's shake (its amplitude)
 *   groundDebris                       dust, chunks and embers flung along the ground; water spray
 *   wall, frontDistance                the shockwave's wall of dust and embers, so far upwind
 *   stormDust                          thick billows rushing past the camera
 *   debrisRain                         the moon's debris raining out of the sky, some head-on
 *   dust, dustVeil                     the dust fog, and its veil over the picture
 *   rain, lensWater                    wind-driven rain, drops on the lens
 *   lightning, thunderVolume           the chance of a strike in the dust (each 0.7 s), thunder after it
 *   fires                              how many fires the debris has lit round the player
 *   gusts, gustVolume                  gusts rushing past (rate multiplier, loudness)
 *   booms, boomVolume                  debris coming down further off (per second, loudness)
 *   thuds                              debris thudding down close by (per second)
 *   gain                               the loudness of all its sounds (and the timeline's)
 *   fadeOut                            the picture and the sound fading out
 *   debrisArrival                      when the debris reaches the Earth (the sky's shell)
 *
 * It places the timeline's sound anchors "front" (the shockwave) and
 * "upwind". The hole's "quitAfter" still fades out and closes the game.
 */
class Aftermath
{
    public:
        ~Aftermath();
        /* once a frame, after the timeline advanced and before the renderer
         * prepares it; true when the ending is over and the game should close */
        bool update(Camera *camera, Timeline &timeline);

    private:
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
        float m_masterVolume = -1.0f;     /* before the ending's fade (-1: not fading) */

        std::vector<ParticleEmitter *> m_debris;
        bool m_debrisShown = false;

        int m_gusts = 0, m_booms = 0, m_thuds = 0, m_lastBolt = -1;
        float m_nextGust = -1.0f, m_nextBoom = -1.0f, m_nextThud = -1.0f;
        struct Delayed { float at; std::string sound; float volume, pitch; };
        std::vector<Delayed> m_delayed;   /* thunder, after its lightning */

        struct Fire { std::vector<ParticleEmitter *> emitters; PointLight *light; float litAt, size; };
        std::vector<Fire> m_fires;
};

#endif // AFTERMATH_H
