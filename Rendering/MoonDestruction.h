#ifndef MOONDESTRUCTION_H
#define MOONDESTRUCTION_H

#include <glm/glm.hpp>
#include <vector>

#include "Aftermath.h"

class BlackHole;
class Shader;
class Timeline;

/* The Moon's destruction (a Kugelblitz in the sky's black hole, eating the Moon from inside): its
 * own element. Plays it (from the hole's settings: wait, ignite, collapse at its speed; or as a
 * timeline drives it) and hands its state to the sky shader, which draws it with
 * res/shaders/moonDestruction.glsl (#included there, called through its hooks). */
class MoonDestruction
{
    public:
        /* once a frame. skyHole: the sky's black hole (nullptr: none); timelineShell /
         * timelineEjecta: the timeline's debris shell and matter thrown at us (< 0: not set, they
         * follow the Moon); timeScale: 0 pauses it (the timeline editor) */
        void update(BlackHole *skyHole, const glm::vec3 &moonDirection, float renderTime, float timeScale,
                    float timelineShell, float timelineEjecta, Shader &sky);

        /* its own timeline lanes (the blast), joined to the list the timeline editor shows */
        static const std::vector<Aftermath::Parameter> &lanes();
        /* once a frame: its lanes as the timeline has them now */
        void readTimeline(const Timeline &timeline);

        /* matter flying straight at us out of the hole (0..1): the sky's pass over the hole */
        float frontEjecta() const { return m_frontEjecta; }
        /* how far its debris shell has got, as drawn (0..2) */
        float shownShell() const { return m_shownShell; }

    private:
        float m_lastTime = -1.0f;
        float m_frontEjecta = 0.0f;
        float m_shownShell = 0.0f;
        float m_blast = 0.0f;
        float m_blastAngle = 0.0f;     /* degrees round the moon's disc, as seen from here: 0 right, 90 up */
};

#endif // MOONDESTRUCTION_H
