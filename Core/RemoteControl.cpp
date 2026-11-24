#include "RemoteControl.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

RemoteControl::~RemoteControl()
{
    if (m_socket >= 0) close(m_socket);
}

bool RemoteControl::connect(int port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t) port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);     /* localhost only */
    if (::connect(s, (sockaddr *) &address, sizeof(address)) != 0) {
        std::printf("Remote: could not connect to the editor on port %d\n", port);
        close(s);
        return false;
    }
    m_socket = s;
    std::printf("Remote: connected to the editor on port %d\n", port);
    return true;
}

std::vector<std::string> RemoteControl::poll()
{
    std::vector<std::string> lines;
    if (m_socket < 0) return lines;
    char buffer[65536];
    for (;;) {
        ssize_t n = recv(m_socket, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (n > 0) {
            m_pending.append(buffer, (size_t) n);
            continue;
        }
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            /* the editor went away */
            close(m_socket);
            m_socket = -1;
        }
        break;
    }
    size_t at;
    while ((at = m_pending.find('\n')) != std::string::npos) {
        if (at > 0) lines.push_back(m_pending.substr(0, at));
        m_pending.erase(0, at + 1);
    }
    return lines;
}

bool RemoteControl::sendAll(const void *data, size_t size)
{
    const char *p = (const char *) data;
    while (size > 0 && m_socket >= 0) {
        ssize_t n = send(m_socket, p, size, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(m_socket);
            m_socket = -1;
            return false;
        }
        p += n;
        size -= (size_t) n;
    }
    return m_socket >= 0;
}

void RemoteControl::sendState(const std::string &json)
{
    std::string line = "S " + json + "\n";
    sendAll(line.data(), line.size());
}

void RemoteControl::sendParameters(const std::string &json)
{
    std::string line = "P " + json + "\n";
    sendAll(line.data(), line.size());
}

void RemoteControl::sendFrame(int width, int height, const unsigned char *rgba)
{
    if (m_socket < 0) return;
    m_rgb.resize((size_t) width * height * 3);
    for (int y = 0; y < height; y++) {
        const unsigned char *src = rgba + (size_t) (height - 1 - y) * width * 4;   /* GL: bottom-up */
        unsigned char *dst = m_rgb.data() + (size_t) y * width * 3;
        for (int x = 0; x < width; x++) {
            dst[x * 3 + 0] = src[x * 4 + 0];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4 + 2];
        }
    }
    char header[64];
    int n = std::snprintf(header, sizeof(header), "F %d %d %zu\n", width, height, m_rgb.size());
    if (sendAll(header, (size_t) n)) sendAll(m_rgb.data(), m_rgb.size());
}
