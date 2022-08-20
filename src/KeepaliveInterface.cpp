#include "KeepaliveInterface.hpp"

KeepaliveInterface::KeepaliveInterface(EspApi& esp, uint8_t pin,
        unsigned interval, unsigned resetInterval)
        : esp(esp), pin(pin), interval(interval), resetInterval(resetInterval) {
    esp.pinMode(pin, GpioMode::output);
}

void KeepaliveInterface::start() {
    this->reset();
}

void KeepaliveInterface::execute(const std::string& /*command*/) {
}

void KeepaliveInterface::update(Actions /*action*/) {
    if (esp.millis() > this->nextReset) {
        this->reset();
    }
}

void KeepaliveInterface::reset() {
        esp.digitalWrite(this->pin, 0);
        esp.delay(this->resetInterval);
        esp.digitalWrite(this->pin, 1);
        this->nextReset = esp.millis() + this->interval;
}

