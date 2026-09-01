#ifndef COMMON_CYCLETIMER_HPP
#define COMMON_CYCLETIMER_HPP

#include "EspApi.hpp"

class CycleTimer {
public:
    explicit CycleTimer(EspApi& esp);
    void tick();
    void reset();
    /** Test/diagnostic API: number of ticks counted since last reset(). */
    unsigned long getCycles() const;
    unsigned long getMaxCycleTime() const;
    float getAvgCycleTime() const;

private:
    EspApi& esp;
    unsigned long previousCycle = 0;
    unsigned long sumCycleTime = 0;
    unsigned long maxCycleTime = 0;
    unsigned long cycles = 0;
    bool started = false;
};

#endif  // COMMON_CYCLETIMER_HPP
