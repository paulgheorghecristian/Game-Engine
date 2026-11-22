#include "FrameRecorder.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sys/stat.h>

FrameRecorder::~FrameRecorder()
{
    stop();
}

bool FrameRecorder::start(int width, int height)
{
    if (m_pipe != nullptr) {
        return true;
    }
    m_width = width;
    m_height = height;

    /* ~/Videos/ge-YYYYmmdd-HHMMSS.mp4 */
    const char *home = std::getenv("HOME");
    std::string dir = std::string(home != nullptr ? home : ".") + "/Videos";
    mkdir(dir.c_str(), 0755);
    char stamp[32];
    std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
    m_path = dir + "/ge-" + stamp + ".mp4";

    const char *encoder = std::getenv("GE_RECORD_ENCODER");
    std::string codec = encoder != nullptr ? encoder : "h264_nvenc";
    std::string quality = codec == "h264_nvenc" ? "-preset p5 -cq 20" : "-preset veryfast -crf 18";
    /* raw RGBA frames on stdin, bottom-up (OpenGL), flipped by ffmpeg */
    std::string command = "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgba -s " +
                          std::to_string(width) + "x" + std::to_string(height) + " -r " + std::to_string(FPS) +
                          " -i - -vf vflip -c:v " + codec + " " + quality + " -pix_fmt yuv420p \"" + m_path + "\"";
    m_pipe = popen(command.c_str(), "w");
    if (m_pipe == nullptr) {
        std::cout << "Recording: could not start ffmpeg" << std::endl;
        return false;
    }

    size_t bytes = (size_t) width * height * 4;
    glGenBuffers(2, m_pbos);
    for (GLuint pbo : m_pbos) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, bytes, nullptr, GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    m_index = 0;
    m_pending = false;
    m_framesWritten = 0;
    m_startTime = std::chrono::steady_clock::now();
    std::cout << "Recording to " << m_path << " (F12 to stop)" << std::endl;
    return true;
}

float FrameRecorder::elapsedSeconds() const
{
    if (m_pipe == nullptr) {
        return 0.0f;
    }
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - m_startTime).count();
}

void FrameRecorder::writePending()
{
    if (!m_pending) {
        return;
    }
    GLuint pbo = m_pbos[1 - m_index];
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    const void *pixels = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (pixels != nullptr) {
        size_t bytes = (size_t) m_width * m_height * 4;
        for (int i = 0; i < m_pendingCopies; i++) {
            fwrite(pixels, 1, bytes, m_pipe);
        }
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        m_framesWritten += m_pendingCopies;
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    m_pending = false;
}

void FrameRecorder::captureFrame()
{
    if (m_pipe == nullptr) {
        return;
    }
    /* how many video frames this rendered frame stands for, in real time */
    long due = (long) (elapsedSeconds() * FPS) + 1;
    int copies = (int) std::max(0L, due - m_framesWritten - (m_pending ? m_pendingCopies : 0));

    /* the previous frame's copy has had a whole frame to finish: write it */
    writePending();
    if (copies <= 0) {
        return;   /* rendering faster than 60 fps: this frame is dropped */
    }

    /* start the copy of this frame (finishes while the next one renders) */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[m_index]);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    m_pending = true;
    m_pendingCopies = copies;
    m_index = 1 - m_index;
}

void FrameRecorder::stop()
{
    if (m_pipe == nullptr) {
        return;
    }
    writePending();
    pclose(m_pipe);
    m_pipe = nullptr;
    glDeleteBuffers(2, m_pbos);
    m_pbos[0] = m_pbos[1] = 0;
    std::cout << "Recording saved to " << m_path << " (" << m_framesWritten / (float) FPS << " s)" << std::endl;
}
