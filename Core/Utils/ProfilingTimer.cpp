#include "ProfilingTimer.h"

ProfilingTimer::ProfilingTimer(const std::string &name) : total (0), name (name), maxTime(0), numOfCalls(0) {

}

void ProfilingTimer::begin() {
    numOfCalls++;
    startTime = HighResolutionClock::now();
}

void ProfilingTimer::end() {
    auto endTime = HighResolutionClock::now();
    unsigned int passedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

    total += passedTime;

    if (passedTime > maxTime) {
        maxTime = passedTime;
    }
}

void ProfilingTimer::reset() {
    total = 0;
    numOfCalls = 0;
    maxTime = 0;
}

void ProfilingTimer::print() {
    std::cout << ((float) total / 1000) / numOfCalls << "(max=" << ((float) maxTime / 1000) << ")" << " milliseconds passed for " << name << " timer" << std::endl;
}

ProfilingTimer::~ProfilingTimer()
{
    //dtor
}
