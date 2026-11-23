#ifndef AFTERMATH_H
#define AFTERMATH_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

class Camera;
class ParticleEmitter;

/*
 * What follows the sky's Kugelblitz: once the moon is gone and the black
 * hole has formed (its collapse reaches 1 while playing), the world reacts.
 *
 *   0 - 8 s   an eerie calm, in silence (no sound crosses space): the wind
 *             dies and turns to blow from the hole; from 0.6 s the moon's
 *             debris lands far off and sweeps in from the horizon (a wall of
 *             dust and embers); its roar is heard only in the last 1.5 s
 *   8 s       it reaches the player (BlackHole::DEBRIS_ARRIVAL): a deep
 *             blast, the camera shakes
 *   8 - 14 s  a gale from the hole: trees and grass thrash, the water heaves,
 *             fog rolls in, clouds race, and dust, burning embers and debris
 *             are flung at the player (roaring wind, gusts, spray)
 *   14 - 32 s it settles into a windier, foggier new normal; or, with the
 *             hole's "dustOut", it only gets worse (louder, deeper, more
 *             gusts, booms and thunder) and thickens into a dust storm
 *             (lightning, rain) until nothing is left but dust
 *
 * With the hole's "quitAfter" set, that many seconds after the hole forms
 * the picture and sound fade out (2 s) and the game closes.
 *
 * It runs on the Kugelblitz's playback clock, so it is the same every time
 * and Restart / Reset take it back. A map that starts with the moon already
 * gone gets none of it.
 */
class Aftermath
{
    public:
        ~Aftermath();
        /* once a frame, before the renderer prepares it; true when the
         * ending is over and the game should close */
        bool update(Camera *camera);

    private:
        void reset();
        void startDebris();
        void placeDebris(Camera *camera, const glm::vec3 &downwind, float strength, float wall, float frontAhead,
                         float dust, float falling, float t);
        /* the storm's sounds, at its loudness (m_gain) */
        int play(const std::string &name, float volume, float pitch = 1.0f, bool loop = false);
        int play3D(const std::string &name, const glm::vec3 &position, float volume, float radius, float pitch = 1.0f,
                   bool loop = false);
        void setVolume(int voice, float volume);
        float m_gain = 1.0f;          /* the dust-out storm (00) is louder */
        bool m_rainOnly = false;      /* the dust-out storm: only matter raining down, nothing thrown along the ground */

        bool m_active = false;
        float m_completedAt = 0.0f;        /* playback-clock time the hole formed */
        float m_lastPlayTime = 0.0f;
        float m_lastCollapse = -1.0f;
        glm::vec2 m_stormDirection = glm::vec2(1.0f, 0.0f);   /* xz, from the hole towards the player */
        glm::vec3 m_holeDirection = glm::vec3(0.0f, 1.0f, 0.0f); /* towards the hole in the sky */

        bool m_impactPlayed = false, m_secondBoomPlayed = false;
        int m_gusts = 0;
        float m_nextGust = 0.0f;
        int m_roarVoice = 0, m_sprayVoice = 0;
        float m_masterVolume = -1.0f;     /* before the ending's fade (-1: not fading) */

        std::vector<ParticleEmitter *> m_debris;
        bool m_debrisShown = false;

        /* the dust storm (dustOut): lightning, thunder after it, the wind's
         * own howl over the roar, debris thudding down */
        struct Delayed { float at; std::string sound; float volume, pitch; };
        std::vector<Delayed> m_delayed;
        int m_lastBolt = -1;
        int m_windVoice = 0, m_rumbleVoice = 0;
        float m_nextBoom = 0.0f;
        int m_booms = 0;
        float m_nextThud = 0.0f;
        int m_thuds = 0;
};

#endif // AFTERMATH_H
