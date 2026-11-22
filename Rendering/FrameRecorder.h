#ifndef FRAME_RECORDER_H
#define FRAME_RECORDER_H

#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include <GL/glew.h>

/*
 * Records what the engine renders to an MP4 (F12): each frame is read back
 * from the default framebuffer asynchronously (two pixel buffer objects, so
 * the GPU is not stalled waiting for the copy) and piped to ffmpeg, which
 * encodes it with NVENC (or x264 with GE_RECORD_ENCODER=libx264).
 *
 * The video runs at a constant 60 fps in real time: frames are dropped when
 * the game renders faster and repeated when it renders slower, so playback
 * speed matches what was seen. Capture happens before the overlays (profiler
 * bars, ImGui) are drawn, so the recording is clean.
 *
 * The game's sound is captured from the mixer meanwhile; on stop it is
 * written to a WAV and muxed with the video by ffmpeg in the background
 * (the video stream is copied, the sound encoded to AAC).
 *
 * Desktop recorders often capture OpenGL windows as black (GNOME on X11 with
 * the NVIDIA driver); this does not depend on them.
 */
class FrameRecorder
{
    public:
        FrameRecorder() = default;
        ~FrameRecorder();

        FrameRecorder(const FrameRecorder &) = delete;
        FrameRecorder &operator=(const FrameRecorder &) = delete;

        /* start recording frames of this size; false if ffmpeg could not start */
        bool start(int width, int height);
        /* ends the video; with sound (interleaved stereo float), it is muxed in */
        void stop(const std::vector<float> &audio = std::vector<float>(), int sampleRate = 44100);
        bool isRecording() const { return m_pipe != nullptr; }
        /* call once per frame with the finished image in the bound default framebuffer */
        void captureFrame();
        float elapsedSeconds() const;
        const std::string &outputPath() const { return m_path; }

        static const int FPS = 60;

    private:
        void writePending();

        FILE *m_pipe = nullptr;
        std::string m_path;
        std::string m_videoPath;         /* the video alone, until the sound is added */
        int m_width = 0, m_height = 0;
        GLuint m_pbos[2] = {0, 0};
        int m_index = 0;
        bool m_pending = false;          /* the other PBO holds a frame not yet written */
        int m_pendingCopies = 0;         /* how many times to write it (real-time pacing) */
        long m_framesWritten = 0;
        std::chrono::steady_clock::time_point m_startTime;
};

#endif // FRAME_RECORDER_H
