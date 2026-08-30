#ifndef COMMON_CYCLETIMER_HPP
#define COMMON_CYCLETIMER_HPP

#include "EspApi.hpp"

class CycleTimer {
public:
    explicit CycleTimer(EspApi& esp);
    void tick();
    void reset();
    unsigned long getCycles() const;
    unsigned long getMaxCycleTime() const;

private:
    EspApi& esp;
    unsigned long previousCycle = 0;
    unsigned long maxCycleTime = 0;
    unsigned long cycles = 0;
    bool started = false;
};

#endif  // COMMON_CYCLETIMER_HPP