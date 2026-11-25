#include "ProfilingTimer.h"

#include "BarGUI.h"
#include <algorithm>
#include <vector>
#include <memory>

#define CPU_START_X 1500
#define CPU_START_Y 100
#define GPU_START_X 200
#define GPU_START_Y 300
#define Y_OFFSET 100

std::unordered_map<std::string, ProfilingTimer> ProfilingTimer::timers;
long ProfilingTimer::s_frame = 0;
glm::vec2 ProfilingTimer::lastPositionGPU(GPU_START_X, GPU_START_Y);
glm::vec2 ProfilingTimer::lastPositionCPU(CPU_START_X, CPU_START_Y);
bool ProfilingTimer::syncGPU = false;

ProfilingTimer::ProfilingTimer(BarGUI *barGUI, Type type) : total(0),
                                                  maxTime(0),
                                                  numOfCalls(0),
                                                  passedTime(0),
                                                  uptr_barGUI(barGUI),
                                                  m_type(type),
                                                  m_timerQuery(0),
                                                  m_timerQuery1(0),
                                                  m_timerQuery2(0) {
    if (type == GPU) {
        glGenQueries(1, &m_timerQuery1);
        glGenQueries(1, &m_timerQuery2);
        m_timerQuery = m_timerQuery1;
    }
}

void ProfilingTimer::begin() {
    if (m_type == CPU) {
        numOfCalls++;
        if (syncGPU) glFinish();
        startTime = HighResolutionClock::now();
    } else if (m_type == GPU) {
        glBeginQuery(GL_TIME_ELAPSED, m_timerQuery);
    } else {
        assert(false);
    }
}

void ProfilingTimer::end() {

    if (m_type == CPU) {
        if (syncGPU) glFinish();
        auto endTime = HighResolutionClock::now();
        passedTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        total += passedTime;
        if (m_frame != s_frame) {
            m_frame = s_frame;
            m_frameTime = 0.0;
        }
        m_frameTime += passedTime;

        if (passedTime > maxTime) {
            maxTime = passedTime;
        }
    } else if (m_type == GPU) {
        glEndQuery(GL_TIME_ELAPSED);
    } else {
        assert(false);
    }
}

void ProfilingTimer::reset() {
    total = 0;
    numOfCalls = 0;
    maxTime = 0;
}

void ProfilingTimer::printThis (const std::string &name) {
    std::cout << ((double) total) / numOfCalls << "(max=" << ((double) maxTime) << ")" << " milliseconds passed for " << name << " timer" << std::endl;
}

double ProfilingTimer::getPassedTimeAvg() {
    /* warning: if m_type is GPU, make sure this
     * function is always called after heavy GPU operations
     * to not stall the OpenGL pipeline
    */

    if (m_type == CPU) {
        if (numOfCalls == 0 || total == 0) {
            return 0;
        }
        return total / numOfCalls;
    } else if (m_type == GPU) {
        GLuint64 nanoseconds = 0;
        double milliseconds = 0;
        GLint done = GL_FALSE;

        if (m_timerQuery == m_timerQuery1) {
            m_timerQuery = m_timerQuery2;
        } else {
            m_timerQuery = m_timerQuery1;
        }

        glGetQueryObjectui64v(m_timerQuery, GL_QUERY_RESULT, &nanoseconds);
        milliseconds = nanoseconds * 0.000001;

        return milliseconds;
    } else {
        assert(false);
        return 0;
    }
}

void ProfilingTimer::begin(const std::string &name, Type type) {
    auto search = timers.find(name);

    if (search != timers.end()) {
        search->second.begin();
    } else {
        if (!timers.empty()) {
            if (type == CPU) {
                lastPositionCPU.y += Y_OFFSET;
            } else if (type == GPU) {
                lastPositionGPU.y += Y_OFFSET;
            } else {
                assert(false);
            }
        }

        ProfilingTimer newTimer(new BarGUI(type == CPU ? lastPositionCPU : lastPositionGPU,
                                          glm::vec2(0, 0.5),
                                          glm::vec3(1, 0, 0),
                                          16.0f,
                                          true,
                                          glm::vec2(0),
                                          name),
                                type);
        newTimer.begin();
        timers.insert(std::make_pair(name, std::move(newTimer)));
    }
}

void ProfilingTimer::end(const std::string &name) {
    auto search = timers.find(name);

    if (search != timers.end()) {
        search->second.end();
    } else {
        assert(false);
    }
}

void ProfilingTimer::reset(const std::string &name) {
    auto search = timers.find(name);

    if (search != timers.end()) {
        search->second.reset();
    } else {
        assert(false);
    }
}

void ProfilingTimer::print (const std::string &name) {
    auto search = timers.find(name);

    if (search != timers.end()) {
        search->second.printThis(name);
    }  else {
        assert(false);
    }
}

void ProfilingTimer::renderAllBarGUIs () {
    for (auto &bar : timers) {
        if (bar.second.getType() == CPU) {
            double time = bar.second.getPassedTimeAvg();
            bar.second.uptr_barGUI->update(time);
        }
        bar.second.uptr_barGUI->render();
    }
}

void ProfilingTimer::updateGPUTimers() {
    for (auto &bar : timers) {
        if (bar.second.getType() == GPU) {
            double time = bar.second.getPassedTimeAvg();
            bar.second.uptr_barGUI->update(time);
        }
    }
}

void ProfilingTimer::resetAll() {
    for (auto &it : timers) {
        it.second.reset();
    }
}

void ProfilingTimer::printAll() {
    /* the slowest first */
    std::vector<std::pair<double, const std::string *>> order;
    for (auto &it : timers) {
        if (it.second.getType() == CPU && it.second.numOfCalls > 0) {
            order.push_back({ it.second.total / it.second.numOfCalls, &it.first });
        }
    }
    std::sort(order.begin(), order.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    for (const auto &o : order) {
        const ProfilingTimer &timer = timers.at(*o.second);
        printf("  %-28s avg=%7.3fms max=%7.3fms\n", o.second->c_str(), o.first, timer.maxTime);
    }
}

void ProfilingTimer::printFrame(double minMs) {
    std::vector<std::pair<double, const std::string *>> order;
    for (auto &it : timers) {
        if (it.second.getType() == CPU && it.second.m_frame == s_frame && it.second.m_frameTime >= minMs) {
            order.push_back({ it.second.m_frameTime, &it.first });
        }
    }
    std::sort(order.begin(), order.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    for (const auto &o : order) {
        printf("    %-28s %7.3fms\n", o.second->c_str(), o.first);
    }
}

ProfilingTimer &ProfilingTimer::getTimer(const std::string &name)
{
    auto search = timers.find(name);

    if (search != timers.end()) {
        return search->second;
    } else {
        assert(false);
    }
}

ProfilingTimer &ProfilingTimer::operator=(ProfilingTimer &&otherTimer)
{
    if (this != &otherTimer) {
        delete this;
        new (this) ProfilingTimer(std::move(otherTimer));
    }

    return *this;
}

ProfilingTimer::ProfilingTimer(ProfilingTimer &&otherTimer)
{
    m_timerQuery1 = otherTimer.m_timerQuery1;
    otherTimer.m_timerQuery1 = 0;

    m_timerQuery2 = otherTimer.m_timerQuery2;
    otherTimer.m_timerQuery2 = 0;

    uptr_barGUI = std::move(otherTimer.uptr_barGUI);
    m_type = otherTimer.m_type;

    total = otherTimer.total;
    maxTime = otherTimer.maxTime;
    numOfCalls = otherTimer.numOfCalls;
    passedTime = otherTimer.passedTime;
    startTime = otherTimer.startTime;
    m_timerQuery = (otherTimer.m_timerQuery == otherTimer.m_timerQuery2) ? m_timerQuery2 : m_timerQuery1;
}

ProfilingTimer::~ProfilingTimer()
{
    if (m_type == GPU) {
        if (m_timerQuery1 > 0) {
            glDeleteQueries(1, &m_timerQuery1);
        }

        if (m_timerQuery2 > 0) {
            glDeleteQueries(1, &m_timerQuery2);
        }
    }
}
