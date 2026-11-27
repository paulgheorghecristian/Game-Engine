#include "SoundScape.h"

#include <iostream>

#include "AudioMaster.h"
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

}
