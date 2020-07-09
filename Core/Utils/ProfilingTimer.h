#ifndef PROFILINGTIMER_H
#define PROFILINGTIMER_H

#include <chrono>
#include <string>
#include <iostream>
#include <unordered_map>
#include <glm/glm.hpp>
#include <memory>
#include <GL/glew.h>

class BarGUI;
typedef std::chrono::high_resolution_clock HighResolutionClock;

#define PT_FromHere_GPU(timerName) \
    do {               \
        ProfilingTimer::begin(timerName, ProfilingTimer::GPU); \
    } while (0)

#define PT_ToHere_GPU(timerName) \
    do {               \
        ProfilingTimer::end(timerName); \
    } while (0)

#define PT_FromHere(timerName) \
    do {               \
        ProfilingTimer::begin(timerName); \
    } while (0)

#define PT_ToHere(timerName) \
    do {               \
        ProfilingTimer::end(timerName); \
    } while (0)

#define PT_Reset(timerName) \
    do {            \
        ProfilingTimer::reset(timerName); \
    } while (0)

#define PT_Print(timerName) \
    do {            \
        ProfilingTimer::print(timerName); \
    } while (0)

#define PT_PrintAndReset(timerName) \
    do {            \
        ProfilingTimer::print(timerName); \
        ProfilingTimer::reset(timerName); \
    } while (0)

class ProfilingTimer
{
    public:
        enum Type {
            CPU = 0,
            GPU
        };

        ProfilingTimer(BarGUI *barGUI, Type type = CPU);
        ~ProfilingTimer();

        ProfilingTimer &operator= (const ProfilingTimer &otherTimer) = delete;
        ProfilingTimer (const ProfilingTimer &otherTimer) = delete;

        ProfilingTimer &operator= (ProfilingTimer &&otherTimer);
        ProfilingTimer (ProfilingTimer &&otherTimer);

        void begin ();
        void end ();
        void reset ();
        void printThis (const std::string &name);
        double getPassedTimeAvg();
        inline const Type &getType() const { return m_type; }
        inline GLuint &getTimerQuery() { assert(m_type == GPU); return m_timerQuery; }

        static void begin (const std::string &name, Type type = CPU);
        static void end (const std::string &name);
        static void reset (const std::string &name);
        static void print (const std::string &name);
        static void renderAllBarGUIs();
        static void updateGPUTimers();

        static ProfilingTimer &getTimer(const std::string &name);
    protected:
    private:
        unsigned int total, numOfCalls, maxTime;
        unsigned int passedTime;
        std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
        std::unique_ptr<BarGUI> uptr_barGUI;
        Type m_type;
        GLuint m_timerQuery, m_timerQuery1, m_timerQuery2; /* for GPU type, swap them to prevent waiting */

        static std::unordered_map<std::string, ProfilingTimer> timers;
        static glm::vec2 lastPositionGPU, lastPositionCPU;
};

#endif // PROFILINGTIMER_H
