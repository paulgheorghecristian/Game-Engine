#include "Timeline.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include "AudioMaster.h"
#include "BlackHole.h"
#include "RenderingMaster.h"

Timeline::~Timeline()
{
    /* the audio is shut down with the engine; nothing to stop here */
}

float Timeline::Track::evaluate(float t) const
{
    if (keys.empty()) return 0.0f;
    if (t <= keys.front().t) return keys.front().v;
    if (t >= keys.back().t) return keys.back().v;
    size_t i = 1;
    while (i < keys.size() && keys[i].t < t) i++;
    const Key &a = keys[i - 1], &b = keys[i];
    float u = (t - a.t) / std::max(b.t - a.t, 1e-6f);
    if (curve == 1) u = u * u * (3.0f - 2.0f * u);
    else if (curve == 2) u = 0.0f;
    return a.v + (b.v - a.v) * u;
}

/* a number (constant), an array of [t, v] (smooth), or {"keys": [...], "curve": ...} */
Timeline::Track Timeline::parseTrack(const rapidjson::Value &value)
{
    Track track;
    const rapidjson::Value *keys = nullptr;
    if (value.IsNumber()) {
        track.keys.push_back({ 0.0f, value.GetFloat() });
        return track;
    }
    if (value.IsArray()) {
        keys = &value;
    } else if (value.IsObject()) {
        if (value.HasMember("keys")) keys = &value["keys"];
        if (value.HasMember("curve") && value["curve"].IsString()) {
            std::string curve = value["curve"].GetString();
            track.curve = curve == "linear" ? 0 : (curve == "step" ? 2 : 1);
        }
    }
    if (keys != nullptr && keys->IsArray()) {
        for (const auto &key : keys->GetArray()) {
            if (key.IsArray() && key.Size() == 2 && key[0].IsNumber() && key[1].IsNumber()) {
                track.keys.push_back({ key[0].GetFloat(), key[1].GetFloat() });
            }
        }
        std::sort(track.keys.begin(), track.keys.end(), [](const Key &a, const Key &b) { return a.t < b.t; });
    }
    return track;
}

void Timeline::loadOne(const rapidjson::Value &json)
{
    if (!json.IsObject()) return;
    Sequence sequence;
    if (json.HasMember("name") && json["name"].IsString()) sequence.name = json["name"].GetString();
    sequence.trigger = json.HasMember("trigger") && json["trigger"].IsString() ? json["trigger"].GetString() : "mapStart";
    if (json.HasMember("params") && json["params"].IsObject()) {
        for (auto it = json["params"].MemberBegin(); it != json["params"].MemberEnd(); ++it) {
            sequence.params[it->name.GetString()] = parseTrack(it->value);
        }
    }
    if (json.HasMember("sounds") && json["sounds"].IsArray()) {
        for (const auto &s : json["sounds"].GetArray()) {
            if (!s.IsObject() || !s.HasMember("sound") || !s["sound"].IsString()) continue;
            Sound sound;
            sound.sound = s["sound"].GetString();
            if (s.HasMember("at") && s["at"].IsNumber()) sound.at = s["at"].GetFloat();
            sound.volume = s.HasMember("volume") ? parseTrack(s["volume"]) : Track{ { { 0.0f, 1.0f } }, 1 };
            if (s.HasMember("pitch") && s["pitch"].IsNumber()) sound.pitch = s["pitch"].GetFloat();
            if (s.HasMember("loop") && s["loop"].IsBool()) sound.loop = s["loop"].GetBool();
            if (s.HasMember("anchor") && s["anchor"].IsString()) sound.anchor = s["anchor"].GetString();
            if (s.HasMember("radius") && s["radius"].IsNumber()) sound.radius = s["radius"].GetFloat();
            sequence.sounds.push_back(sound);
        }
    }
    if (json.HasMember("events") && json["events"].IsArray()) {
        for (const auto &e : json["events"].GetArray()) {
            if (e.IsObject() && e.HasMember("event") && e["event"].IsString()) {
                sequence.events.push_back({ e.HasMember("at") && e["at"].IsNumber() ? e["at"].GetFloat() : 0.0f,
                                            e["event"].GetString() });
            }
        }
    }
    m_sequences.push_back(sequence);
}

void Timeline::load(const rapidjson::Value &sequences)
{
    if (sequences.IsArray()) {
        for (const auto &sequence : sequences.GetArray()) loadOne(sequence);
    } else {
        loadOne(sequences);
    }
}

bool Timeline::loadFile(const std::string &path)
{
    std::ifstream file(path);
    if (!file) {
        std::cout << "Timeline: no sequence file " << path << std::endl;
        return false;
    }
    std::stringstream text;
    text << file.rdbuf();
    rapidjson::Document json;
    if (json.Parse(text.str().c_str()).HasParseError()) {
        std::cout << "Timeline: could not parse " << path << std::endl;
        return false;
    }
    load(json);
    return true;
}

void Timeline::clear()
{
    for (Sequence &sequence : m_sequences) stopSounds(sequence);
    m_sequences.clear();
    m_params.clear();
    m_anchors.clear();
    m_fired.clear();
    m_running = false;
    m_time = -1.0f;
    m_mapStart = -1.0f;
}

void Timeline::stopSounds(Sequence &sequence)
{
    AudioMaster *audio = AudioMaster::getInstance();
    for (Sound &sound : sequence.sounds) {
        if (sound.voice != 0) audio->stop(sound.voice);
        sound.voice = 0;
        sound.started = false;
    }
    for (Event &event : sequence.events) event.done = false;
}

void Timeline::advance(float renderTime)
{
    m_params.clear();
    m_fired.clear();
    m_running = false;
    m_time = -1.0f;
    if (m_mapStart < 0.0f) m_mapStart = renderTime;

    /* the sky's Moon destruction: its playback clock, from the moment it finished */
    BlackHole *kugelblitz = nullptr;
    for (BlackHole *hole : RenderingMaster::getInstance()->blackHoles) {
        if (hole->sky && hole->kugelblitz) { kugelblitz = hole; break; }
    }
    for (Sequence &sequence : m_sequences) {
        float t = -1.0f;
        if (sequence.trigger == "kugelblitzFormed") {
            if (kugelblitz != nullptr && kugelblitz->formedAt >= 0.0f) t = kugelblitz->playTime - kugelblitz->formedAt;
        } else {
            t = renderTime - m_mapStart;
        }
        /* not triggered (yet, or any more), or rewound: back to the start */
        if (t < 0.0f || (sequence.running && t < sequence.t - 0.05f)) {
            if (sequence.running) stopSounds(sequence);
            sequence.running = false;
            sequence.t = -1.0f;
            if (t < 0.0f) continue;
        }
        sequence.running = true;
        sequence.t = t;
        m_running = true;
        m_time = t;
        for (auto &param : sequence.params) m_params[param.first] = param.second.evaluate(t);
        for (Event &event : sequence.events) {
            if (!event.done && t >= event.at) {
                event.done = true;
                m_fired.push_back(event.name);
            }
        }
    }
}

void Timeline::playSounds(const glm::vec3 &eye)
{
    AudioMaster *audio = AudioMaster::getInstance();
    if (!audio->isAvailable()) return;
    float gain = get("gain", 1.0f);
    for (Sequence &sequence : m_sequences) {
        if (!sequence.running) continue;
        float t = sequence.t;
        for (Sound &sound : sequence.sounds) {
            if (t < sound.at) continue;
            auto anchor = m_anchors.find(sound.anchor);
            bool spatial = !sound.anchor.empty();
            glm::vec3 position = anchor != m_anchors.end() ? anchor->second : eye;
            float volume = sound.volume.evaluate(t) * gain;
            if (!sound.started) {
                sound.started = true;
                /* a one-shot only if it is still its moment (not long after, on a late start) */
                if (!sound.loop && t > sound.at + 0.5f) continue;
                sound.voice = spatial ? audio->play3D(sound.sound, position, volume, sound.radius, sound.pitch, sound.loop)
                                      : audio->play(sound.sound, volume, sound.pitch, sound.loop);
                if (!sound.loop) sound.voice = 0;
            } else if (sound.loop && sound.voice != 0) {
                audio->setVolume(sound.voice, volume);
                if (spatial) audio->setPosition(sound.voice, position);
            }
        }
    }
}

float Timeline::get(const std::string &name, float fallback) const
{
    auto it = m_params.find(name);
    return it != m_params.end() ? it->second : fallback;
}

bool Timeline::fired(const std::string &event) const
{
    return std::find(m_fired.begin(), m_fired.end(), event) != m_fired.end();
}
