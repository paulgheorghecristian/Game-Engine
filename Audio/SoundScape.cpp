#include "SoundScape.h"

#include <iostream>

#include "AudioMaster.h"
#include "BlackHole.h"
#include "Camera.h"
#include "RenderingMaster.h"

static float jsonNumber(const rapidjson::Value &json, const char *key, float def)
{
    return json.HasMember(key) && json[key].IsNumber() ? json[key].GetFloat() : def;
}

void SoundScape::load(const rapidjson::Value &json)
{
    clear();
    if (!json.IsArray()) {
        if (!json.IsNull()) std::cout << "SoundScape: \"Audio\" is a list of sounds (see Audio/SoundScape.h): ignored" << std::endl;
        return;
    }
    for (const auto &s : json.GetArray()) {
        if (!s.IsObject() || !s.HasMember("sound") || !s["sound"].IsString()) continue;
        Source source;
        source.sound = s["sound"].GetString();
        if (s.HasMember("position") && s["position"].IsArray() && s["position"].Size() == 3) {
            const auto &p = s["position"].GetArray();
            if (p[0].IsNumber() && p[1].IsNumber() && p[2].IsNumber()) {
                source.position = glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat());
                source.placed = true;
            }
        }
        source.volume = jsonNumber(s, "volume", 1.0f);
        source.radius = jsonNumber(s, "radius", 300.0f);
        source.pitch = jsonNumber(s, "pitch", 1.0f);
        if (s.HasMember("loop") && s["loop"].IsBool()) source.loop = s["loop"].GetBool();
        if (s.HasMember("follows") && s["follows"].IsString()) {
            std::string follows = s["follows"].GetString();
            if (follows == "wind") source.follows = WIND;
            else if (follows == "night") source.follows = NIGHT;
            else std::cout << "SoundScape: " << source.sound << " follows \"" << follows << "\": not something it can follow (wind, night)" << std::endl;
        }
        m_sources.push_back(source);
    }
}

void SoundScape::clear()
{
    AudioMaster *audio = AudioMaster::getInstance();
    for (Source &s : m_sources) audio->stop(s.voice);
    m_sources.clear();
    m_started = false;
}

/* its volume now: its own, with what it follows */
float SoundScape::loudness(const Source &source) const
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    switch (source.follows) {
        case WIND: return source.volume * glm::clamp(0.4f + 0.6f * rm->getMapWind(), 0.0f, 1.6f);
        case NIGHT: return source.volume * rm->getNightFactor();
        default: return source.volume;
    }
}

/* the debris booms: at these points of the collapse (during the death
 * spiral), as the torn-off crust reaches the atmosphere */
static const float BOOM_AT[] = { 0.66f, 0.72f, 0.77f, 0.81f, 0.85f, 0.88f, 0.91f, 0.94f, 0.97f };
static const int BOOMS = sizeof(BOOM_AT) / sizeof(BOOM_AT[0]);

void SoundScape::update(Camera *camera)
{
    AudioMaster *audio = AudioMaster::getInstance();
    if (!audio->isAvailable() || camera == nullptr) {
        return;
    }
    audio->setListener(camera->getPosition(), camera->getForward(), glm::vec3(0, 1, 0));
    RenderingMaster *rm = RenderingMaster::getInstance();

    if (!m_started) {
        m_started = true;
        for (Source &s : m_sources) {
            s.voice = s.placed ? audio->play3D(s.sound, s.position, loudness(s), s.radius, s.pitch, s.loop)
                               : audio->play(s.sound, loudness(s), s.pitch, s.loop);
        }
    }
    for (Source &s : m_sources) {
        if (s.follows != STEADY && s.voice != 0) audio->setVolume(s.voice, loudness(s));
    }

    /* the sky's Kugelblitz */
    BlackHole *kugelblitz = nullptr;
    for (BlackHole *hole : rm->blackHoles) {
        if (hole->sky && hole->kugelblitz) { kugelblitz = hole; break; }
    }
    if (kugelblitz == nullptr) {
        return;
    }
    if (kugelblitz->silent || kugelblitz->driven) {
        /* no sound crosses space: nothing until its debris arrives (Core/Aftermath); or a
         * timeline plays the Moon's destruction: its sounds are the timeline's */
        m_lastCollapse = kugelblitz->collapse;
        m_lastPlayTime = kugelblitz->playTime;
        return;
    }
    /* ignition: the flash first, the deep blast a few seconds later */
    const float BLAST_DELAY = 6.5f;   /* seconds */
    if (kugelblitz->playTime < m_lastPlayTime || kugelblitz->playTime > m_lastPlayTime + 0.5f) {
        /* Restart / Reset, or a jump in time: a pending blast is cancelled, what is still
         * sounding belongs to where we were */
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
        m_oneShots.push_back(audio->play("blast", 1.0f));
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
        /* nearer the end, louder and more often: more debris coming in */
        float volume = 0.7f + 0.3f * (float) m_booms / (BOOMS - 1);
        float pitch = 0.9f + 0.15f * (float) ((m_booms * 7) % 5) / 4.0f;
        if (m_oneShots.size() > 16) m_oneShots.erase(m_oneShots.begin());
        m_oneShots.push_back(audio->play("boom_" + std::to_string(1 + m_booms % 3), volume, pitch));
        m_booms++;
    }
    m_lastCollapse = collapse;
}
