#ifndef AFTERMATH_H
#define AFTERMATH_H

#include <vector>
#include <glm/glm.hpp>

class Camera;
class ParticleEmitter;

/*
 * What follows the sky's Kugelblitz: once the moon is gone and the black
 * hole has formed (its collapse reaches 1 while playing), the world reacts.
 *
 *   0 - 3.4 s an eerie calm: the wind dies and turns to blow from the hole;
 *             from 0.6 s the shockwave sweeps in from the horizon (a wall of
 *             dust and embers, its roar growing as it nears)
 *   3.4 s     it reaches the player: a deep blast, the camera shakes
 *   3.4 - 14 s a gale from the hole: trees and grass thrash, the water heaves,
 *             fog rolls in, clouds race, and dust, burning embers and debris
 *             are flung at the player (roaring wind, gusts, spray)
 *   14 - 32 s it settles into a windier, foggier new normal
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
        void placeDebris(Camera *camera, const glm::vec3 &downwind, float strength, float wall, float frontAhead);

        bool m_active = false;
        float m_completedAt = 0.0f;        /* playback-clock time the hole formed */
        float m_lastPlayTime = 0.0f;
        float m_lastCollapse = -1.0f;
        glm::vec2 m_stormDirection = glm::vec2(1.0f, 0.0f);   /* xz, from the hole towards the player */

        bool m_impactPlayed = false, m_secondBoomPlayed = false;
        int m_gusts = 0;
        float m_nextGust = 0.0f;
        int m_roarVoice = 0, m_sprayVoice = 0;
        float m_masterVolume = -1.0f;     /* before the ending's fade (-1: not fading) */

        std::vector<ParticleEmitter *> m_debris;
        bool m_debrisShown = false;
};

#endif // AFTERMATH_H
