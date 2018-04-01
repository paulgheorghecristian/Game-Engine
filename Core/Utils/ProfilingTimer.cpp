#include "ProfilingTimer.h"

#include "BarGUI.h"
#include <memory>

std::unordered_map<std::string, ProfilingTimer> ProfilingTimer::timers;
glm::vec2 ProfilingTimer::lastPosition;

ProfilingTimer::ProfilingTimer(BarGUI *barGUI) : total (0),
                                                  maxTime(0),
                                                  numOfCalls(0),
                                                  passedTime (0),
                                                  uptr_barGUI (barGUI) {
    startTime = HighResolutionClock::now();
}

void ProfilingTimer::begin() {
    numOfCalls++;
    startTime = HighResolutionClock::now();
}

void ProfilingTimer::end() {
    auto endTime = HighResolutionClock::now();
    passedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

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

void ProfilingTimer::printThis (const std::string &name) {
    std::cout << ((double) total) / numOfCalls << "(max=" << ((double) maxTime) << ")" << " milliseconds passed for " << name << " timer" << std::endl;
}

double ProfilingTimer::getPassedTimeAvg() {
    if (numOfCalls == 0 || total == 0) {
        return 0;
    }
    return (double) total / numOfCalls;
}

void ProfilingTimer::begin (const std::string &name) {
    auto search = timers.find (name);

    if (search != timers.end ()) {
        search->second.begin ();
    } else {
        if (timers.empty ()) {
            lastPosition = glm::vec2 (START_X, START_Y);
            timers.insert (std::make_pair (name, ProfilingTimer (new BarGUI (lastPosition,
                                                                              glm::vec2 (0, 0.5),
                                                                              glm::vec3 (1, 0, 0),
                                                                              glm::vec2 (0),
                                                                              name))));
        } else {
            lastPosition.y += Y_OFFSET;
            timers.insert (std::make_pair (name, ProfilingTimer (new BarGUI (lastPosition,
                                                                              glm::vec2 (0, 0.5),
                                                                              glm::vec3 (1, 0, 0),
                                                                              glm::vec2 (0),
                                                                              name))));
        }
    }
}

void ProfilingTimer::end (const std::string &name) {
    auto search = timers.find (name);

    if (search != timers.end ()) {
        search->second.end ();
    } else {
        assert (false);
    }
}

void ProfilingTimer::reset (const std::string &name) {
    auto search = timers.find (name);

    if (search != timers.end ()) {
        search->second.reset ();
    } else {
        assert (false);
    }
}

void ProfilingTimer::print (const std::string &name) {
    auto search = timers.find (name);

    if (search != timers.end ()) {
        search->second.printThis (name);
    }  else {
        assert (false);
    }
}

void ProfilingTimer::renderAllBarGUIs () {
    for (auto &bar : timers) {
        bar.second.uptr_barGUI->render();
    }
}

void ProfilingTimer::updateAllBarGUIs () {
    for (auto &bar : timers) {
        double time = bar.second.getPassedTimeAvg();
        bar.second.uptr_barGUI->update ((void *) &time);
    }
}

ProfilingTimer::~ProfilingTimer()
{
    //dtor
}
