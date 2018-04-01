#ifndef PROFILINGTIMER_H
#define PROFILINGTIMER_H

#include <chrono>
#include <string>
#include <iostream>
#include <unordered_map>
#include <glm/glm.hpp>
#include <memory>

class BarGUI;
typedef std::chrono::high_resolution_clock HighResolutionClock;

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
        ProfilingTimer(BarGUI *barGUI);
        ~ProfilingTimer();

        ProfilingTimer &operator= (const ProfilingTimer &otherTimer) = delete;
        ProfilingTimer (const ProfilingTimer &otherTimer) = delete;

        ProfilingTimer &operator= (ProfilingTimer &&otherTimer) = default;
        ProfilingTimer (ProfilingTimer &&otherTimer) = default;

        void begin ();
        void end ();
        void reset ();
        void printThis (const std::string &name);
        double getPassedTimeAvg();

        static void begin (const std::string &name);
        static void end (const std::string &name);
        static void reset (const std::string &name);
        static void print (const std::string &name);
        static void renderAllBarGUIs ();
        static void updateAllBarGUIs ();
    protected:
    private:
        unsigned int total, numOfCalls, maxTime;
        unsigned int passedTime;
        std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
        std::unique_ptr<BarGUI> uptr_barGUI;

        static std::unordered_map<std::string, ProfilingTimer> timers;
        static glm::vec2 lastPosition;
};

#endif // PROFILINGTIMER_H
