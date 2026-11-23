#include "SoundScape.h"

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
    if (!json.IsObject()) {
        return;
    }
    m_wind = jsonNumber(json, "wind", 0.0f);
    m_crickets = jsonNumber(json, "crickets", 0.0f);
    if (json.HasMember("sources") && json["sources"].IsArray()) {
        for (const auto &s : json["sources"].GetArray()) {
            if (!s.HasMember("sound") || !s.HasMember("position")) continue;
            Source source;
            source.sound = s["sound"].GetString();
            const auto &p = s["position"].GetArray();
            source.position = glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat());
            source.volume = jsonNumber(s, "volume", 1.0f);
            source.radius = jsonNumber(s, "radius", 300.0f);
            m_sources.push_back(source);
        }
    }
}

void SoundScape::clear()
{
    AudioMaster *audio = AudioMaster::getInstance();
    for (Source &s : m_sources) audio->stop(s.voice);
    audio->stop(m_windVoice);
    audio->stop(m_cricketVoice);
    m_sources.clear();
    m_windVoice = m_cricketVoice = 0;
    m_started = false;
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
    const Environment &env = rm->getEnvironment();

    if (!m_started) {
        m_started = true;
        if (m_wind > 0.0f) m_windVoice = audio->play("wind_loop", 0.0f, 1.0f, true);
        if (m_crickets > 0.0f) m_cricketVoice = audio->play("crickets_loop", 0.0f, 1.0f, true);
        for (Source &s : m_sources) {
            s.voice = audio->play3D(s.sound, s.position, s.volume, s.radius, 1.0f, true);
        }
    }
    /* wind with the map's wind; crickets only at night */
    if (m_windVoice != 0) audio->setVolume(m_windVoice, m_wind * glm::clamp(0.4f + 0.6f * env.wind, 0.0f, 1.6f));
    if (m_cricketVoice != 0) audio->setVolume(m_cricketVoice, m_crickets * rm->getNightFactor());

    /* the sky's Kugelblitz */
    BlackHole *kugelblitz = nullptr;
    for (BlackHole *hole : rm->blackHoles) {
        if (hole->sky && hole->kugelblitz) { kugelblitz = hole; break; }
    }
    if (kugelblitz == nullptr) {
        return;
    }
    if (kugelblitz->silent) {
        /* no sound crosses space: nothing until its debris arrives (Core/Aftermath) */
        m_lastCollapse = kugelblitz->collapse;
        m_lastPlayTime = kugelblitz->playTime;
        return;
    }
    /* ignition: the flash first, the deep blast a few seconds later */
    const float BLAST_DELAY = 6.5f;   /* seconds */
    if (kugelblitz->playTime < m_lastPlayTime) {
        m_blastDue = -1.0f;           /* Restart / Reset: a pending blast is cancelled */
    }
    m_lastPlayTime = kugelblitz->playTime;
    if (kugelblitz->ignitionTime > m_lastIgnition + 0.5f) {
        m_blastDue = kugelblitz->playTime + BLAST_DELAY;
        m_lastIgnition = kugelblitz->ignitionTime;
    }
    if (m_blastDue >= 0.0f && kugelblitz->playTime >= m_blastDue) {
        audio->play("blast", 1.0f);
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
        audio->play("boom_" + std::to_string(1 + m_booms % 3), volume, pitch);
        m_booms++;
    }
    m_lastCollapse = collapse;
}
