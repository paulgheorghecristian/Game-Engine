#include "KugelblitzSounds.h"

#include "AudioMaster.h"
#include "BlackHole.h"
#include "RenderingMaster.h"

/* the debris booms: at these points of the collapse (during the death spiral), as the torn-off crust
 * reaches the atmosphere */
static const float BOOM_AT[] = { 0.66f, 0.72f, 0.77f, 0.81f, 0.85f, 0.88f, 0.91f, 0.94f, 0.97f };
static const int BOOMS = sizeof(BOOM_AT) / sizeof(BOOM_AT[0]);

void KugelblitzSounds::update()
{
    AudioMaster *audio = AudioMaster::getInstance();
    if (!audio->isAvailable()) return;
    BlackHole *kugelblitz = nullptr;
    for (BlackHole *hole : RenderingMaster::getInstance()->blackHoles) {
        if (hole->sky && hole->kugelblitz) { kugelblitz = hole; break; }
    }
    if (kugelblitz == nullptr) return;
    if (kugelblitz->silent || kugelblitz->driven) {
        /* no sound crosses space: nothing until its debris arrives (Core/Aftermath); or a timeline
         * plays the Moon's destruction: its sounds are the timeline's */
        m_lastCollapse = kugelblitz->collapse;
        m_lastPlayTime = kugelblitz->playTime;
        return;
    }
    /* ignition: the flash first, the deep blast a few seconds later */
    const float BLAST_DELAY = 6.5f;   /* seconds */
    if (kugelblitz->playTime < m_lastPlayTime || kugelblitz->playTime > m_lastPlayTime + 0.5f) {
        /* Restart / Reset, or a jump in time: a pending blast is cancelled, what is still sounding
         * belongs to where we were */
        if (kugelblitz->playTime < m_lastPlayTime) m_blastDue = -1.0f;
        for (int voice : m_oneShots) audio->cut(voice);
        m_oneShots.clear();
    }
    m_lastPlayTime = kugelblitz->playTime;
    if (kugelblitz->ignitionTime > m_lastIgnition + 0.5f) {
        m_blastDue = kugelblitz->playTime + BLAST_DELAY;
        m_lastIgnition = kugelblitz->ignitionTime;
    }
    if (m_blastDue >= 0.0f && kugelblitz->playTime >= m_blastDue) {
        if (!kugelblitz->blastSound.empty()) m_oneShots.push_back(audio->play(kugelblitz->blastSound, 1.0f));
        m_blastDue = -1.0f;
    }
    /* booms as the collapse passes their points (rewinding re-arms them) */
    float collapse = kugelblitz->collapse;
    if (collapse < m_lastCollapse - 0.01f) {
        m_booms = 0;
        while (m_booms < BOOMS && BOOM_AT[m_booms] <= collapse) m_booms++;
    }
    if (m_lastCollapse < 0.0f) {
        /* a map starting part-way: the booms already past stay silent */
        while (m_booms < BOOMS && BOOM_AT[m_booms] <= collapse) m_booms++;
    }
    while (m_booms < BOOMS && collapse >= BOOM_AT[m_booms]) {
        /* nearer the end, louder and more often: more debris coming in; the hole's booms in turn */
        float volume = 0.7f + 0.3f * (float) m_booms / (BOOMS - 1);
        float pitch = 0.9f + 0.15f * (float) ((m_booms * 7) % 5) / 4.0f;
        if (m_oneShots.size() > 16) m_oneShots.erase(m_oneShots.begin());
        const std::vector<std::string> &booms = kugelblitz->boomSounds;
        if (!booms.empty()) m_oneShots.push_back(audio->play(booms[m_booms % booms.size()], volume, pitch));
        m_booms++;
    }
    m_lastCollapse = collapse;
}
