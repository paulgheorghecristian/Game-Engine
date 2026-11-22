#ifndef AUDIO_MASTER_H
#define AUDIO_MASTER_H

#include <map>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <SDL.h>

/*
 * Sound: a small software mixer on SDL2's own audio device (no extra
 * library). Sounds are WAV files (res/sounds, made by tools/make_sounds.py),
 * converted on load to 44.1 kHz float. Voices play once or loop, with a
 * volume that ramps (no clicks), a pitch (playback rate), a stereo pan, and
 * optionally a position in the world: then their loudness and pan follow
 * the listener (the camera) each frame.
 *
 * The mixing runs on SDL's audio thread; everything else is called from
 * the game thread and takes the device lock briefly.
 */
class AudioMaster
{
    public:
        static AudioMaster *getInstance();
        bool isAvailable() const { return m_device != 0; }

        /* loads res/sounds/<name>.wav once; false if missing */
        bool load(const std::string &name);
        /* plays a sound; returns a voice id (0 if it could not play) */
        int play(const std::string &name, float volume = 1.0f, float pitch = 1.0f, bool loop = false);
        /* a sound at a place in the world: louder near the listener, panned */
        int play3D(const std::string &name, const glm::vec3 &position, float volume, float radius,
                   float pitch = 1.0f, bool loop = false);
        /* one of name_1..name_count at random, with a little pitch variety */
        int playVariant(const std::string &name, int count, float volume, float pitchVariation = 0.08f,
                        float basePitch = 1.0f);

        void setVolume(int voice, float volume);    /* ramps there over ~50 ms */
        void setPosition(int voice, const glm::vec3 &position);   /* a 3D voice moves */
        void stop(int voice);                       /* fades out, then ends */
        bool isPlaying(int voice) const;

        /* the listener (camera); call once a frame: updates 3D voices */
        void setListener(const glm::vec3 &position, const glm::vec3 &forward, const glm::vec3 &up);
        void setMasterVolume(float volume) { m_master = volume; }
        /* recording (F12): keeps a copy of everything mixed from now on */
        void startCapture();
        /* the mix since startCapture: interleaved stereo float at sampleRate() */
        std::vector<float> stopCapture();
        static int sampleRate();
        float getMasterVolume() const { return m_master; }

        ~AudioMaster();

    private:
        AudioMaster();
        struct Sound {
            std::vector<float> samples;   /* mono, 44.1 kHz */
        };
        struct Voice {
            int id = 0;
            const Sound *sound = nullptr;
            double position = 0.0;        /* in samples */
            float pitch = 1.0f;
            bool loop = false;
            float volume = 0.0f, target = 0.0f;   /* current / wanted (ramped) */
            float pan = 0.0f;             /* -1 left .. 1 right */
            bool stopping = false;
            bool spatial = false;
            glm::vec3 worldPosition = glm::vec3(0.0f);
            float radius = 1.0f, baseVolume = 1.0f;
        };
        static void callback(void *user, Uint8 *stream, int bytes);
        void mix(float *out, int frames);
        void spatialize(Voice &voice) const;

        SDL_AudioDeviceID m_device = 0;
        std::map<std::string, Sound> m_sounds;
        std::vector<Voice> m_voices;
        int m_nextId = 1;
        float m_master = 0.8f;
        glm::vec3 m_listener = glm::vec3(0.0f), m_listenerRight = glm::vec3(1, 0, 0);
        bool m_capturing = false;
        std::vector<float> m_capture;
};

#endif // AUDIO_MASTER_H
