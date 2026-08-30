#include "CycleTimer.hpp"

CycleTimer::CycleTimer(EspApi& esp) : esp(esp) {}

void CycleTimer::tick() {
    const auto now = this->esp.millis();
    ++this->cycles;
    if (this->started) {
        const auto cycleTime = now - this->previousCycle;
        if (cycleTime > this->maxCycleTime) {
            this->maxCycleTime = cycleTime;
        }
    }
    this->started = true;
    this->previousCycle = now;
}

void CycleTimer::reset() {
    this->started = false;
    this->cycles = 0;
    this->maxCycleTime = 0;
}

unsigned long CycleTimer::getCycles() const {
    return this->cycles;
}

unsigned long CycleTimer::getMaxCycleTime() const {
    return this->maxCycleTime;
}