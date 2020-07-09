#include "ProfilingTimer.h"

#include "BarGUI.h"
#include <memory>

#define CPU_START_X 1500
#define CPU_START_Y 100
#define GPU_START_X 200
#define GPU_START_Y 300
#define Y_OFFSET 100

std::unordered_map<std::string, ProfilingTimer> ProfilingTimer::timers;
glm::vec2 ProfilingTimer::lastPositionGPU(GPU_START_X, GPU_START_Y);
glm::vec2 ProfilingTimer::lastPositionCPU(CPU_START_X, CPU_START_Y);

ProfilingTimer::ProfilingTimer(BarGUI *barGUI, Type type) : total(0),
                                                  maxTime(0),
                                                  numOfCalls(0),
                                                  passedTime(0),
                                                  uptr_barGUI(barGUI),
                                                  m_type(type) {
    if (type == GPU) {
        glGenQueries(1, &m_timerQuery1);
        glGenQueries(1, &m_timerQuery2);
        m_timerQuery = m_timerQuery1;
    }
}

void ProfilingTimer::begin() {
    if (m_type == CPU) {
        numOfCalls++;
        startTime = HighResolutionClock::now();
    } else if (m_type == GPU) {
        glBeginQuery(GL_TIME_ELAPSED, m_timerQuery);
    } else {
        assert(false);
    }
}

void ProfilingTimer::end() {

    if (m_type == CPU) {
        auto endTime = HighResolutionClock::now();
        passedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        total += passedTime;

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
        return (double) total / numOfCalls;
    } else if (m_type == GPU) {
        GLuint64 nanoseconds;
        double milliseconds;
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
