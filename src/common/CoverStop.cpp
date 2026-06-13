#include "CoverStop.hpp"

CoverStop::CoverStop(
    EspApi& esp, uint8_t pin, bool latching, bool invertOutput,
    std::ostream& debug, std::string debugPrefix)
    : esp(esp)
    , pin(pin)
    , latching(latching)
    , invertOutput(invertOutput)
    , debug(debug)
    , debugPrefix(std::move(debugPrefix)) {
    if (this->isLatching()) {
        this->esp.pinMode(this->pin, GpioMode::output);
        this->stop();
    }
}

void CoverStop::stop() {
    if (!this->isLatching()) {
        return;
    }

    this->debug << this->debugPrefix << "stop" << std::endl;
    this->triggered = true;
    this->esp.digitalWrite(this->pin, this->invertOutput ? 0 : 1);
}

void CoverStop::reset() {
    if (!this->isLatching()) {
        return;
    }

    this->debug << this->debugPrefix << "Reset stop" << std::endl;
    this->esp.digitalWrite(this->pin, this->invertOutput ? 1 : 0);
    this->triggered = false;
}

bool CoverStop::isTriggered() const {
    return this->triggered;
}

bool CoverStop::isLatching() const {
    return this->latching;
}
