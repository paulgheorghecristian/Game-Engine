#include "Timeline.h"

#include <algorithm>
#include <cmath>
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

/* y of the cubic bezier (0,0) (x1,y1) (x2,y2) (1,1) at x (as CSS's
 * cubic-bezier, and the animation editor's curves) */
static float unitBezier(float x1, float y1, float x2, float y2, float x)
{
    float cx = 3.0f * x1, bx = 3.0f * (x2 - x1) - cx, ax = 1.0f - cx - bx;
    float cy = 3.0f * y1, by = 3.0f * (y2 - y1) - cy, ay = 1.0f - cy - by;
    float s = x;
    for (int i = 0; i < 8; i++) {                 /* Newton */
        float fx = ((ax * s + bx) * s + cx) * s - x;
        if (std::fabs(fx) < 1e-5f) break;
        float d = (3.0f * ax * s + 2.0f * bx) * s + cx;
        if (std::fabs(d) < 1e-6f) break;
        s -= fx / d;
    }
    if (s < 0.0f || s > 1.0f) {                   /* bisection, if it left the range */
        float lo = 0.0f, hi = 1.0f;
        s = x;
        for (int i = 0; i < 24; i++) {
            float fx = ((ax * s + bx) * s + cx) * s;
            if (std::fabs(fx - x) < 1e-5f) break;
            if (fx < x) lo = s; else hi = s;
            s = (lo + hi) * 0.5f;
        }
    }
    return ((ay * s + by) * s + cy) * s;
}

float Timeline::Track::evaluate(float t) const
{
    if (keys.empty()) return 0.0f;
    if (t <= keys.front().t) return keys.front().v;
    if (t >= keys.back().t) return keys.back().v;
    size_t i = 1;
    while (i < keys.size() && keys[i].t < t) i++;
    const Key &a = keys[i - 1], &b = keys[i];
    if (a.hold || (curve == 2 && !a.handles)) return a.v;
    float u = (t - a.t) / std::max(b.t - a.t, 1e-6f);
    if (a.handles || b.handles) {
        /* a key without its own handles: the track's curve's */
        float x1 = a.handles ? a.rx : 1.0f / 3.0f, y1 = a.handles ? a.ry : (curve == 0 ? 1.0f / 3.0f : 0.0f);
        float x2 = b.handles ? b.lx : 2.0f / 3.0f, y2 = b.handles ? b.ly : (curve == 0 ? 2.0f / 3.0f : 1.0f);
        u = unitBezier(x1, y1, x2, y2, u);
    } else if (curve == 1) {
        u = u * u * (3.0f - 2.0f * u);
    }
    return a.v + (b.v - a.v) * u;
}

/* a number (constant), an array of [t, v] (smooth), or {"keys": [...], "curve": ...} */
Timeline::Track Timeline::parseTrack(const rapidjson::Value &value)
{
    Track track;
    const rapidjson::Value *keys = nullptr;
    if (value.IsNumber()) {
        Key k;
        k.t = 0.0f;
        k.v = value.GetFloat();
        track.keys.push_back(k);
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
            if (!key.IsArray() || key.Size() < 2 || !key[0].IsNumber() || !key[1].IsNumber()) continue;
            Key k;
            k.t = key[0].GetFloat();
            k.v = key[1].GetFloat();
            if (key.Size() >= 6 && key[2].IsNumber() && key[3].IsNumber() && key[4].IsNumber() && key[5].IsNumber()) {
                k.lx = key[2].GetFloat(); k.ly = key[3].GetFloat();
                k.rx = key[4].GetFloat(); k.ry = key[5].GetFloat();
                k.handles = true;
            }
            for (rapidjson::SizeType i = 2; i < key.Size(); i++) {
                if (key[i].IsString() && std::string(key[i].GetString()) == "hold") k.hold = true;
            }
            track.keys.push_back(k);
        }
        std::sort(track.keys.begin(), track.keys.end(), [](const Key &a, const Key &b) { return a.t < b.t; });
    }
    return track;
}

void Timeline::loadOne(const rapidjson::Value &json)
{
    m_reached.clear();
    if (!json.IsObject()) return;
    Sequence sequence;
    if (json.HasMember("name") && json["name"].IsString()) sequence.name = json["name"].GetString();
    sequence.trigger = json.HasMember("trigger") && json["trigger"].IsString() ? json["trigger"].GetString() : "mapStart";
    if (json.HasMember("length") && json["length"].IsNumber()) sequence.length = json["length"].GetFloat();
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
            if (s.HasMember("volume")) {
                sound.volume = parseTrack(s["volume"]);
            } else {
                Key full;
                full.t = 0.0f;
                full.v = 1.0f;
                sound.volume.keys.push_back(full);
            }
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
        for (const auto &sequence : sequences.GetArray()) load(sequence);
    } else if (sequences.IsObject() && sequences.HasMember("file") && sequences["file"].IsString()) {
        loadFile(sequences["file"].GetString());     /* {"file": "Game/sequences/..."}: kept in its own file */
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

void Timeline::replace(const rapidjson::Value &sequences)
{
    std::vector<Sequence> old;
    old.swap(m_sequences);
    m_reached.clear();          /* (also when the new timeline is empty) */
    load(sequences);
    AudioMaster *audio = AudioMaster::getInstance();
    for (size_t i = 0; i < old.size(); i++) {
        Sequence *now = i < m_sequences.size() ? &m_sequences[i] : nullptr;
        if (now != nullptr && now->trigger == old[i].trigger) {
            now->running = old[i].running;
            now->t = old[i].t;
            for (size_t e = 0; e < now->events.size() && e < old[i].events.size(); e++) {
                if (now->events[e].name == old[i].events[e].name) now->events[e].done = old[i].events[e].done;
            }
        }
        for (size_t j = 0; j < old[i].sounds.size(); j++) {
            Sound &was = old[i].sounds[j];
            Sound *is = now != nullptr && j < now->sounds.size() ? &now->sounds[j] : nullptr;
            if (is != nullptr && is->sound == was.sound && is->loop == was.loop && std::fabs(is->pitch - was.pitch) < 1e-3f &&
                is->anchor == was.anchor) {
                is->started = was.started && is->at <= was.at + 1e-3f;     /* the same sound: it carries on */
                is->voice = is->started ? was.voice : 0;
                if (is->voice == was.voice) was.voice = 0;
            }
            if (was.voice != 0) audio->stop(was.voice);                    /* gone, or another sound now */
        }
    }
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
    m_clock = 0.0f;
    m_lastRender = -1.0f;
    m_reached.clear();
}

void Timeline::stopSounds(Sequence &sequence)
{
    AudioMaster *audio = AudioMaster::getInstance();
    for (Sound &sound : sequence.sounds) {
        /* a loop fades out; a one-shot still sounding (a blast's tail) is cut: it belongs to a
         * moment we are no longer at */
        if (sound.voice != 0) { if (sound.loop) audio->stop(sound.voice); else audio->cut(sound.voice); }
        sound.voice = 0;
        sound.started = false;
    }
    for (Event &event : sequence.events) event.done = false;
}

void Timeline::advance(float renderTime)
{
    m_params.clear();
    if (soloing()) m_params = m_soloOff;        /* (what is not isolated: switched off) */
    m_fired.clear();
    m_running = false;
    m_time = -1.0f;
    if (m_lastRender < 0.0f) m_lastRender = renderTime;
    m_clock += std::max(renderTime - m_lastRender, 0.0f) * m_scale;
    m_lastRender = renderTime;

    /* the sky's Moon destruction: its playback clock, from the moment it finished */
    BlackHole *kugelblitz = nullptr;
    for (BlackHole *hole : RenderingMaster::getInstance()->blackHoles) {
        if (hole->sky && hole->kugelblitz) { kugelblitz = hole; break; }
    }
    for (size_t index = 0; index < m_sequences.size(); index++) {
        Sequence &sequence = m_sequences[index];
        float t = -1.0f;
        if (sequence.trigger == "kugelblitzFormed" && soloing()) {
            t = m_clock - startOf(sequence);        /* (isolating: the Moon may be kept whole; it starts where it is shown) */
        } else if (sequence.trigger == "kugelblitzFormed") {
            if (kugelblitz != nullptr && kugelblitz->formedAt >= 0.0f) t = kugelblitz->playTime - kugelblitz->formedAt;
        } else {
            t = m_clock;
        }
        /* not triggered (yet, or any more), or rewound: back to the start */
        if (t < 0.0f || (sequence.running && t < sequence.t - 0.05f)) {
            if (sequence.running) stopSounds(sequence);
            sequence.running = false;
            sequence.t = -1.0f;
            if (t < 0.0f) continue;
        }
        if (sequence.running && t > sequence.t + 0.5f) {
            /* jumped ahead: the one-shots still sounding belong to where we were */
            AudioMaster *audio = AudioMaster::getInstance();
            for (Sound &sound : sequence.sounds) {
                if (!sound.loop && sound.voice != 0) { audio->cut(sound.voice); sound.voice = 0; }
            }
        }
        sequence.running = true;
        sequence.t = t;
        m_running = true;
        m_time = t;
        for (auto &param : sequence.params) {
            if (soloing() && m_soloOff.count(param.first) && !m_soloParams.count({ (int)index, param.first })) continue;
            m_params[param.first] = param.second.evaluate(t);
        }
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
    for (size_t index = 0; index < m_sequences.size(); index++) {
        Sequence &sequence = m_sequences[index];
        if (!sequence.running) continue;
        float t = sequence.t;
        for (size_t s = 0; s < sequence.sounds.size(); s++) {
            Sound &sound = sequence.sounds[s];
            if (soloing() && !m_soloSounds.count({ (int)index, (int)s })) {
                /* not isolated: not heard (and heard again, from where it is, when nothing is) */
                if (sound.voice != 0) { if (sound.loop) audio->stop(sound.voice); else audio->cut(sound.voice); }
                sound.voice = 0;
                sound.started = false;
                continue;
            }
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
            } else if (sound.voice != 0) {
                /* its loudness follows the curve while it plays (a one-shot too, until it ends) */
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

void Timeline::setSolo(const std::set<std::pair<int, std::string>> &params, const std::set<std::pair<int, int>> &sounds,
                       const std::map<std::string, float> &off)
{
    m_soloParams = params;
    m_soloSounds = sounds;
    m_soloOff = off;
}

bool Timeline::fired(const std::string &event) const
{
    return std::find(m_fired.begin(), m_fired.end(), event) != m_fired.end();
}

float Timeline::firstReach(const std::string &name, float level) const
{
    std::string key = name + "@" + std::to_string(level);
    auto known = m_reached.find(key);
    if (known != m_reached.end()) return known->second;
    /* only the sequences on the scene clock from 0 (not those started by something else) */
    const Track *track = nullptr;
    for (auto it = m_sequences.rbegin(); it != m_sequences.rend(); ++it) {     /* (the last one wins) */
        const Sequence &sequence = *it;
        if (sequence.trigger == "kugelblitzFormed") continue;
        auto param = sequence.params.find(name);
        if (param != sequence.params.end()) { track = &param->second; break; }
    }
    float found = -1.0f;
    if (track != nullptr) {
        float end = 0.0f;
        for (const Key &k : track->keys) end = std::max(end, k.t);
        const float STEP = 0.02f;
        for (float t = 0.0f; t <= end + STEP; t += STEP) {
            if (track->evaluate(t) >= level - 1e-6f) {
                float lo = std::max(t - STEP, 0.0f), hi = t;          /* then to a ten-thousandth */
                if (track->evaluate(lo) >= level - 1e-6f) hi = lo;
                for (int i = 0; i < 14 && hi > lo; i++) {
                    float mid = 0.5f * (lo + hi);
                    if (track->evaluate(mid) >= level - 1e-6f) hi = mid; else lo = mid;
                }
                found = hi;
                break;
            }
        }
    }
    m_reached[key] = found;
    return found;
}

float Timeline::startOf(const Sequence &sequence) const
{
    if (sequence.trigger == "kugelblitzFormed") {
        /* when the sky's black hole has formed: on the timeline, if it sets the collapse; else
         * by the black hole's own playback from the start */
        float formed = firstReach("collapse", 1.0f);
        if (formed >= 0.0f) return formed;
        for (BlackHole *hole : RenderingMaster::getInstance()->blackHoles) {
            if (hole->sky && hole->kugelblitz) return hole->formationTime();
        }
    }
    return 0.0f;            /* "mapStart" (and anything else): the start */
}

std::vector<float> Timeline::starts() const
{
    std::vector<float> out;
    for (const Sequence &sequence : m_sequences) out.push_back(startOf(sequence));
    return out;
}

float Timeline::valueAtScene(const std::string &name, float T, float fallback) const
{
    /* (the last sequence that sets it wins, as when playing: advance() overwrites in order) */
    for (auto it = m_sequences.rbegin(); it != m_sequences.rend(); ++it) {
        const Sequence &sequence = *it;
        float t = T - startOf(sequence);
        if (t < 0.0f) continue;             /* not started by then */
        auto param = sequence.params.find(name);
        if (param != sequence.params.end()) return param->second.evaluate(t);
    }
    return fallback;
}

float Timeline::sceneLength() const
{
    float longest = 0.0f;
    for (const Sequence &sequence : m_sequences) {
        float length = sequence.length;
        if (length <= 0.0f) {
            float last = 0.0f;
            for (const auto &param : sequence.params) {
                if (param.second.keys.size() > 1) last = std::max(last, param.second.keys.back().t);
            }
            for (const Sound &sound : sequence.sounds) {
                last = std::max(last, sound.at);
                if (sound.volume.keys.size() > 1) last = std::max(last, sound.volume.keys.back().t);
            }
            for (const Event &event : sequence.events) last = std::max(last, event.at);
            length = std::ceil((last + 15.0f) / 5.0f) * 5.0f;     /* a while past the last of it, to the next 5 s */
        }
        longest = std::max(longest, startOf(sequence) + length);
    }
    return longest > 0.0f ? longest : 30.0f;
}

float Timeline::firstSoundScene() const
{
    float first = -1.0f;
    for (const Sequence &sequence : m_sequences) {
        for (const Sound &sound : sequence.sounds) {
            float at = startOf(sequence) + sound.at;
            if (first < 0.0f || at < first) first = at;
        }
    }
    return first;
}

std::string Timeline::soundsJson()
{
    /* (Sound's own defaults: volume 1, pitch 1, radius 800) */
    return "{\"volume\": {\"min\": 0, \"max\": 3, \"standard\": 1}, "
           "\"pitch\": {\"min\": 0.2, \"max\": 3, \"standard\": 1}, "
           "\"radius\": {\"min\": 50, \"max\": 5000, \"standard\": 800}, "
           "\"curves\": [\"smooth\", \"linear\", \"step\"]}";
}
