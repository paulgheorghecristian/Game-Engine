#include "AudioMaster.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

static const int RATE = 44100;
static const int MAX_VOICES = 48;
/* volume ramp per sample (~50 ms from silence to full) */
static const float RAMP = 1.0f / (0.05f * RATE);

AudioMaster *AudioMaster::getInstance()
{
    static AudioMaster instance;
    return &instance;
}

AudioMaster::AudioMaster()
{
    /* benchmarks / scripted renders run silent (GE_AUDIO=1 keeps the sound,
     * e.g. with SDL_AUDIODRIVER=disk to test the mixing without speakers) */
    if (std::getenv("GE_MUTE") != nullptr || (std::getenv("GE_BENCH") != nullptr && std::getenv("GE_AUDIO") == nullptr)) {
        return;
    }
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cout << "Audio: " << SDL_GetError() << std::endl;
        return;
    }
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = RATE;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = &AudioMaster::callback;
    want.userdata = this;
    m_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (m_device == 0) {
        std::cout << "Audio: no device (" << SDL_GetError() << ")" << std::endl;
        return;
    }
    SDL_PauseAudioDevice(m_device, 0);
}

AudioMaster::~AudioMaster()
{
    if (m_device != 0) {
        SDL_CloseAudioDevice(m_device);
    }
}

bool AudioMaster::load(const std::string &name)
{
    if (m_sounds.count(name) != 0) {
        return true;
    }
    std::string path = "res/sounds/" + name + ".wav";
    SDL_AudioSpec spec;
    Uint8 *buffer = nullptr;
    Uint32 length = 0;
    if (SDL_LoadWAV(path.c_str(), &spec, &buffer, &length) == nullptr) {
        std::cout << "Audio: cannot load " << path << std::endl;
        return false;
    }
    /* to mono float at 44.1 kHz */
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_F32SYS, 1, RATE);
    std::vector<Uint8> data(length * (cvt.len_mult > 0 ? cvt.len_mult : 1));
    std::copy(buffer, buffer + length, data.begin());
    SDL_FreeWAV(buffer);
    cvt.buf = data.data();
    cvt.len = (int) length;
    if (cvt.needed) {
        SDL_ConvertAudio(&cvt);
    }
    int bytes = cvt.needed ? cvt.len_cvt : (int) length;
    Sound sound;
    sound.samples.assign((float *) data.data(), (float *) (data.data() + bytes));
    SDL_LockAudioDevice(m_device);
    m_sounds[name] = std::move(sound);
    SDL_UnlockAudioDevice(m_device);
    return true;
}

int AudioMaster::play(const std::string &name, float volume, float pitch, bool loop)
{
    if (m_device == 0 || !load(name)) {
        return 0;
    }
    Voice voice;
    voice.id = m_nextId++;
    voice.sound = &m_sounds[name];
    voice.pitch = pitch;
    voice.loop = loop;
    voice.target = volume;
    voice.volume = loop ? 0.0f : volume;   /* loops fade in, one-shots start at once */
    voice.baseVolume = volume;
    SDL_LockAudioDevice(m_device);
    if ((int) m_voices.size() >= MAX_VOICES) {
        /* drop the oldest one-shot */
        for (size_t i = 0; i < m_voices.size(); i++) {
            if (!m_voices[i].loop) {
                m_voices.erase(m_voices.begin() + i);
                break;
            }
        }
    }
    m_voices.push_back(voice);
    SDL_UnlockAudioDevice(m_device);
    return voice.id;
}

int AudioMaster::play3D(const std::string &name, const glm::vec3 &position, float volume, float radius,
                        float pitch, bool loop)
{
    int id = play(name, volume, pitch, loop);
    if (id == 0) {
        return 0;
    }
    SDL_LockAudioDevice(m_device);
    for (Voice &voice : m_voices) {
        if (voice.id == id) {
            voice.spatial = true;
            voice.worldPosition = position;
            voice.radius = std::max(radius, 1.0f);
            spatialize(voice);
            if (!loop) voice.volume = voice.target;
        }
    }
    SDL_UnlockAudioDevice(m_device);
    return id;
}

int AudioMaster::playVariant(const std::string &name, int count, float volume, float pitchVariation)
{
    int pick = 1 + std::rand() % std::max(count, 1);
    float pitch = 1.0f + ((std::rand() % 2001) / 1000.0f - 1.0f) * pitchVariation;
    return play(name + "_" + std::to_string(pick), volume, pitch, false);
}

void AudioMaster::setVolume(int voice, float volume)
{
    if (m_device == 0) return;
    SDL_LockAudioDevice(m_device);
    for (Voice &v : m_voices) {
        if (v.id == voice) {
            v.baseVolume = volume;
            if (!v.spatial) v.target = volume;
            else spatialize(v);
        }
    }
    SDL_UnlockAudioDevice(m_device);
}

void AudioMaster::stop(int voice)
{
    if (m_device == 0) return;
    SDL_LockAudioDevice(m_device);
    for (Voice &v : m_voices) {
        if (v.id == voice) {
            v.stopping = true;
            v.target = 0.0f;
        }
    }
    SDL_UnlockAudioDevice(m_device);
}

bool AudioMaster::isPlaying(int voice) const
{
    for (const Voice &v : m_voices) {
        if (v.id == voice) return true;
    }
    return false;
}

/* loudness falls off with distance (full inside a fifth of the radius,
 * silent at the radius); pan from the side the sound is on */
void AudioMaster::spatialize(Voice &voice) const
{
    glm::vec3 toSound = voice.worldPosition - m_listener;
    float distance = glm::length(toSound);
    float t = glm::clamp((distance - voice.radius * 0.2f) / (voice.radius * 0.8f), 0.0f, 1.0f);
    float attenuation = (1.0f - t) * (1.0f - t);
    voice.target = voice.stopping ? 0.0f : voice.baseVolume * attenuation;
    voice.pan = distance > 1e-3f ? glm::dot(toSound / distance, m_listenerRight) * 0.8f : 0.0f;
}

void AudioMaster::setListener(const glm::vec3 &position, const glm::vec3 &forward, const glm::vec3 &up)
{
    if (m_device == 0) return;
    SDL_LockAudioDevice(m_device);
    m_listener = position;
    glm::vec3 right = glm::cross(forward, up);
    m_listenerRight = glm::length(right) > 1e-4f ? glm::normalize(right) : glm::vec3(1, 0, 0);
    for (Voice &voice : m_voices) {
        if (voice.spatial) spatialize(voice);
    }
    SDL_UnlockAudioDevice(m_device);
}

void AudioMaster::callback(void *user, Uint8 *stream, int bytes)
{
    static_cast<AudioMaster *>(user)->mix(reinterpret_cast<float *>(stream), bytes / (int) (2 * sizeof(float)));
}

void AudioMaster::mix(float *out, int frames)
{
    std::fill(out, out + frames * 2, 0.0f);
    for (Voice &voice : m_voices) {
        const std::vector<float> &s = voice.sound->samples;
        if (s.empty()) {
            voice.stopping = true;
            voice.volume = 0.0f;
            continue;
        }
        /* equal-power pan */
        float angle = (voice.pan + 1.0f) * 0.25f * 3.14159265f;
        float left = std::cos(angle) * 1.41421356f, right = std::sin(angle) * 1.41421356f;
        size_t n = s.size();
        for (int i = 0; i < frames; i++) {
            if (voice.volume < voice.target) voice.volume = std::min(voice.target, voice.volume + RAMP);
            else if (voice.volume > voice.target) voice.volume = std::max(voice.target, voice.volume - RAMP);
            size_t i0 = (size_t) voice.position;
            if (i0 >= n) {
                if (!voice.loop) break;
                voice.position -= (double) n;
                i0 = (size_t) voice.position;
            }
            size_t i1 = i0 + 1 < n ? i0 + 1 : (voice.loop ? 0 : i0);
            float frac = (float) (voice.position - (double) i0);
            float sample = (s[i0] + (s[i1] - s[i0]) * frac) * voice.volume * m_master;
            out[2 * i] += sample * left;
            out[2 * i + 1] += sample * right;
            voice.position += voice.pitch;
        }
    }
    /* finished one-shots and faded-out voices leave */
    m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(), [](const Voice &v) {
        bool ended = !v.loop && v.position >= (double) v.sound->samples.size();
        bool faded = v.stopping && v.volume <= 0.0f;
        return ended || faded;
    }), m_voices.end());
    /* a soft limit instead of hard clipping when many sounds pile up */
    for (int i = 0; i < frames * 2; i++) {
        float x = out[i];
        out[i] = x / (1.0f + std::fabs(x) * 0.25f);
    }
}
