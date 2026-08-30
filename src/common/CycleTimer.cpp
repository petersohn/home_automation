#include "CycleTimer.hpp"

CycleTimer::CycleTimer(EspApi& esp) : esp(esp) {}

void CycleTimer::tick() {
    const auto now = this->esp.millis();
    if (this->started) {
        const auto cycleTime = now - this->previousCycle;
        ++this->cycles;
        this->sumCycleTime += cycleTime;
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
    this->sumCycleTime = 0;
    this->maxCycleTime = 0;
}

unsigned long CycleTimer::getCycles() const {
    return this->cycles;
}

unsigned long CycleTimer::getMaxCycleTime() const {
    return this->maxCycleTime;
}

float CycleTimer::getAvgCycleTime() const {
    return this->cycles == 0
               ? 0
               : static_cast<float>(this->sumCycleTime) / this->cycles;
}