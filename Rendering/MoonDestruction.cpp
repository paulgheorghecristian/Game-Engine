#include "MoonDestruction.h"
#include "BlackHole.h"
#include "Shader.h"
#include "Timeline.h"

#include <algorithm>
#include <cmath>
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

/* its values straight to the sky shader's program (they are its own: not in the sky shader's
 * uniform list) */
static void set(Shader &sky, const char *name, float value)
{
    sky.bind();
    glUniform1f(glGetUniformLocation(sky.getShaderId(), name), value);
}

static void set(Shader &sky, const char *name, const void *value)
{
    sky.bind();
    glUniform2fv(glGetUniformLocation(sky.getShaderId(), name), 1, (const float *) value);
}

const std::vector<Aftermath::Parameter> &MoonDestruction::lanes()
{
    static const std::vector<Aftermath::Parameter> LANES = {
        { "blast",          "Moon", 0.0f, 2.0f, 0.0f, "The lopsided blast as the last of the Moon falls in: part of it thrown out one way (0 = none, 1 = its front far across the sky, 2 = gone)." },
        { "blastDirection", "Moon", -180.0f, 180.0f, 0.0f, "Which way the blast goes, round the Moon as seen from here, in degrees (0 = right, 90 = up)." },
    };
    return LANES;
}

void MoonDestruction::readTimeline(const Timeline &timeline)
{
    m_blast = timeline.get("blast", 0.0f);
    m_blastAngle = timeline.get("blastDirection", 0.0f);
}

void MoonDestruction::update(BlackHole *skyHole, const glm::vec3 &moonDirection, float renderTime, float timeScale,
                             float timelineShell, float timelineEjecta, Shader &sky)
{
    /* a Kugelblitz sits in the moon, and eats it at its own pace */
    float collapse = -1.0f;
    if (skyHole != nullptr && skyHole->kugelblitz) {
        skyHole->direction = moonDirection;
        float dt = m_lastTime < 0.0f ? 0.0f : glm::clamp(renderTime - m_lastTime, 0.0f, 0.1f);   /* hitches (loading) must not eat the countdown */
        dt *= timeScale;   /* 0: paused (the timeline editor) */
        /* playback: from 0 it waits collapseDelay seconds, then ignites
         * and runs at collapseSpeed until the moon is gone */
        if (skyHole->playing) {
            bool waiting = false;
            if (skyHole->collapse <= 0.0f) {
                skyHole->delayElapsed += dt;
                waiting = skyHole->delayElapsed < skyHole->collapseDelay;
            }
            if (!waiting) {
                skyHole->collapse = glm::clamp(skyHole->collapse + skyHole->collapseSpeed / 60.0f * dt, 0.0f, 1.0f);
            }
            if (skyHole->collapse >= 1.0f) {
                skyHole->playing = false;
            }
        }
        collapse = skyHole->collapse;
        /* ignition: the moment collapse leaves 0 (running, or dragged) */
        const float IGNITION = 0.0005f;
        if (skyHole->previousCollapse >= 0.0f && skyHole->previousCollapse <= IGNITION && collapse > IGNITION) {
            skyHole->ignitionTime = renderTime;
        }
        /* the end, watched happening (a map loaded finished has no climax) */
        if (collapse < 1.0f) {
            skyHole->formedAt = -1.0f;
        } else if (skyHole->formedAt < 0.0f && skyHole->previousCollapse >= 0.0f && skyHole->previousCollapse < 1.0f) {
            /* the moment within this frame it reached 1 (not the frame's start: played or jumped
             * to, the same moment then looks the same) */
            float rate = skyHole->collapseSpeed / 60.0f;
            skyHole->formedAt = rate > 0.0f ? skyHole->playTime + (1.0f - skyHole->previousCollapse) / rate
                                            : skyHole->playTime;
        }
        skyHole->previousCollapse = collapse;
        if (!skyHole->driven) skyHole->playTime += dt;      /* (driven: it is the timeline's clock) */
        float playTime = skyHole->playTime;
        set(sky, "collapseTime", playTime);
        float formed = skyHole->formedAt >= 0.0f ? playTime - skyHole->formedAt : -1.0f;
        set(sky, "formedTime", formed);
        /* the debris shell: the timeline's, or following the moon (reaching us DEBRIS_ARRIVAL
         * seconds after it formed, then thinning out over 4 more) */
        const float ARRIVAL = BlackHole::DEBRIS_ARRIVAL;
        float shell = timelineShell >= 0.0f ? timelineShell
                    : formed < 0.0f ? 0.0f
                    : formed <= ARRIVAL ? formed / ARRIVAL : std::min(1.0f + (formed - ARRIVAL) / 4.0f, 2.0f);
        /* ("debrisShell" is not sent: it never was, the sky shader's uniform list lacked it, so the
         * shell is not drawn. Sending it shows it: a change to the picture, not made here) */
        m_shownShell = shell;
        float rate = skyHole->driven && skyHole->drivenRate > 0.0f ? skyHole->drivenRate : skyHole->collapseSpeed / 60.0f;
        set(sky, "collapseRate", rate);
        /* matter thrown straight at us: the timeline's, or following the moon (as the last
         * of it falls in, until DEBRIS_ARRIVAL seconds after it formed: not the timeline's
         * shell, each is on its own) */
        m_frontEjecta = timelineEjecta >= 0.0f ? timelineEjecta
                      : glm::smoothstep(0.85f, 0.95f, collapse) *
                        (formed < 0.0f ? (collapse >= 1.0f ? 0.0f : 1.0f)
                                       : 1.0f - glm::smoothstep(ARRIVAL - 1.5f, ARRIVAL, formed));
        set(sky, "frontEjecta", m_frontEjecta);
        /* the ignition flash and its shock ring: by the destruction's own clock (seconds
         * since it ignited = how far it has got), so it stops when that is paused and is
         * the same wherever it is looked at; dragged by hand (no speed): in real time */
        float flashRate = skyHole->driven && skyHole->drivenRate > 0.0f ? skyHole->drivenRate : skyHole->collapseSpeed / 60.0f;
        float flash = flashRate > 0.0f
            ? (collapse > IGNITION ? std::exp(-collapse / flashRate * 1.4f) : 0.0f)
            : std::exp(-std::max(renderTime - skyHole->ignitionTime, 0.0f) * 1.4f);
        set(sky, "collapseFlash", flash);
        /* the jets leave along the spin axis (the disk's normal), as seen on the moon's disc */
        glm::vec3 moonDir = glm::normalize(moonDirection);
        glm::vec3 right = glm::normalize(glm::cross(moonDir, std::abs(moonDir.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
        glm::vec3 up = glm::cross(right, moonDir);
        glm::vec3 axis = skyHole->diskNormal();
        glm::vec2 jet(glm::dot(axis, right), glm::dot(axis, up));
        jet = glm::length(jet) > 1e-3f ? glm::normalize(jet) : glm::vec2(0.0f, 1.0f);
        set(sky, "collapseSite", (void *) &skyHole->collapseSite);
        set(sky, "holeHorizon", skyHole->horizon);
        set(sky, "jetAxis", (void *) &jet);
        set(sky, "blast", m_blast);
        glm::vec2 blastDirection(std::cos(glm::radians(m_blastAngle)), std::sin(glm::radians(m_blastAngle)));
        set(sky, "blastDirection", (void *) &blastDirection);
    }
    m_lastTime = renderTime;
    set(sky, "collapse", collapse);
    if (collapse < 0.0f) {
        float noFlash = 0.0f, notFormed = -1.0f;
        m_frontEjecta = 0.0f;
        m_shownShell = 0.0f;
        set(sky, "collapseFlash", noFlash);
        set(sky, "formedTime", notFormed);
        set(sky, "blast", 0.0f);
    }
}
