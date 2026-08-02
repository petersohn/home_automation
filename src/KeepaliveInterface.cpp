#include "KeepaliveInterface.hpp"

KeepaliveInterface::KeepaliveInterface(
    EspApi& esp, const KeepaliveConfig& config)
    : esp(esp)
    , pin(config.pin)
    , interval(config.interval)
    , resetInterval(config.resetInterval) {
    esp.pinMode(config.pin, GpioMode::input);
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
