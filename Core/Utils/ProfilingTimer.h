#ifndef PROFILINGTIMER_H
#define PROFILINGTIMER_H

#include <chrono>
#include <string>
#include <iostream>

typedef std::chrono::high_resolution_clock HighResolutionClock;

#define PT_FromHere(timer) \
        do {               \
            timer.begin(); \
        } while (0)

#define PT_ToHere(timer) \
        do {               \
            timer.end(); \
        } while (0)

#define PT_Reset(timer) \
        do {            \
            timer.reset(); \
        } while (0)

#define PT_Print(timer) \
        do {            \
            timer.print(); \
        } while (0)

#define PT_PrintAndReset(timer) \
        do {            \
            timer.print(); \
            timer.reset(); \
        } while (0)

class ProfilingTimer
{
    public:
        ProfilingTimer(const std::string &name);
        void begin();
        void end();
        void reset();
        void print();
        virtual ~ProfilingTimer();
    protected:
    private:
        unsigned int total, numOfCalls, maxTime;
        std::string name;
        std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
};

#endif // PROFILINGTIMER_H
