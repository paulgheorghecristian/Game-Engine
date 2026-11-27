#ifndef KUGELBLITZ_SOUNDS_H
#define KUGELBLITZ_SOUNDS_H

#include <string>
#include <vector>

/*
 * The sky's Kugelblitz heard (one in the world, whatever levels are in it): the flash first, then a
 * deep blast a few seconds after the ignition; booms as the debris of the collapse reaches the
 * atmosphere, at set points of the collapse, nearer the end louder. The sounds are the black hole's
 * own ("blastSound", "boomSounds" in the map's BlackHoles entry: none named, none heard). Silent while
 * the hole is "silent" (no sound crosses space) or "driven" (a timeline plays the Moon's destruction,
 * and its sounds with it).
 */
class KugelblitzSounds
{
    public:
        /* once a frame */
        void update();

    private:
        float m_lastIgnition = -1e9f;
        float m_lastCollapse = -1.0f;
        int m_booms = 0;                      /* how many of the scheduled booms have played */
        float m_blastDue = -1.0f;             /* playback-clock time of the delayed blast (-1: none) */
        float m_lastPlayTime = 0.0f;
        std::vector<int> m_oneShots;          /* the blast and booms still sounding (cut by a jump in time) */
};

#endif // KUGELBLITZ_SOUNDS_H
