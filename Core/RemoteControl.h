#ifndef REMOTECONTROL_H
#define REMOTECONTROL_H

#include <string>
#include <vector>

/*
 * The engine driven from tools/timeline_editor.py (GE_REMOTE=<port>): it
 * connects to the editor on localhost, takes its commands (one JSON object a
 * line: a new timeline, jump to a time, pause, the camera...) and sends it
 * what it renders (raw frames; the editor turns them into a video stream for
 * the browser) and its state.
 *
 * Engine -> editor, a text header line then its payload:
 *   F <width> <height> <bytes>\n  then the frame: RGB, rows top-down
 *   S <json>\n                    the state (the timeline's clock...)
 *   P <json>\n                    once: the list of what a timeline can set (its sliders)
 */
class RemoteControl
{
    public:
        ~RemoteControl();
        bool connect(int port);
        bool connected() const { return m_socket >= 0; }
        /* the command lines received since the last call */
        std::vector<std::string> poll();
        void sendState(const std::string &json);
        /* the list of what a timeline can set (Aftermath::parametersJson) */
        void sendParameters(const std::string &json);
        /* a frame read back from GL (RGBA, rows bottom-up) */
        void sendFrame(int width, int height, const unsigned char *rgba);

    private:
        bool sendAll(const void *data, size_t size);
        int m_socket = -1;
        std::string m_pending;
        std::vector<unsigned char> m_rgb;
};

#endif // REMOTECONTROL_H
