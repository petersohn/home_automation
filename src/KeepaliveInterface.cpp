#include "KeepaliveInterface.hpp"

#include <Arduino.h>

KeepaliveInterface::KeepaliveInterface(int pin, unsigned interval,
        unsigned resetInterval)
        : pin(pin), interval(interval), resetInterval(resetInterval) {
    pinMode(pin, OUTPUT);
}

void KeepaliveInterface::start() {
    this->reset();
}

void KeepaliveInterface::execute(const std::string& /*command*/) {
}

void KeepaliveInterface::update(Actions /*action*/) {
    if (millis() > this->nextReset) {
        this->reset();
    }
}

void KeepaliveInterface::reset() {
        digitalWrite(this->pin, 0);
        delay(this->resetInterval);
        digitalWrite(this->pin, 1);
        this->nextReset = millis() + this->interval;
}

