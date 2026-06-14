#include "CoverStopImpl.hpp"

CoverStopImpl::CoverStopImpl(
    EspApi& esp, CoverState& state, uint8_t pin, bool invertOutput)
    : esp(esp), state(state), pin(pin), invertOutput(invertOutput) {
    if (this->state.latching) {
        this->esp.pinMode(this->pin, GpioMode::output);
        this->stop();
    }
}

void CoverStopImpl::stop() {
    if (!this->state.latching) {
        return;
    }

    this->state.log("stop");
    this->triggered = true;
    this->esp.digitalWrite(this->pin, this->invertOutput ? 0 : 1);
}

void CoverStopImpl::reset() {
    if (!this->state.latching) {
        return;
    }

    this->state.log("Reset stop");
    this->esp.digitalWrite(this->pin, this->invertOutput ? 1 : 0);
    this->triggered = false;
}

bool CoverStopImpl::isTriggered() const {
    return this->triggered;
}
