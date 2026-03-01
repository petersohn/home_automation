#include "KeepaliveInterface.hpp"

KeepaliveInterface::KeepaliveInterface(
    EspApi& esp, uint8_t pin, unsigned interval, unsigned resetInterval)
    : esp(esp), pin(pin), interval(interval), resetInterval(resetInterval) {
    esp.pinMode(pin, GpioMode::input);
}

void KeepaliveInterface::start() {
    this->reset();
}

void KeepaliveInterface::execute(const std::string& /*command*/) {}

void KeepaliveInterface::update(Actions /*action*/) {
    if (this->esp.millis() > this->nextReset) {
        this->reset();
    }
}

void KeepaliveInterface::reset() {
    this->esp.pinMode(this->pin, GpioMode::output);
    this->esp.digitalWrite(this->pin, 0);
    this->esp.delay(this->resetInterval);
    this->esp.pinMode(this->pin, GpioMode::input);
    this->nextReset = this->esp.millis() + this->interval;
}
