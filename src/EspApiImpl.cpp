#include "EspApiImpl.hpp"

#include <Arduino.h>
#include <FunctionalInterrupt.h>
#include <ets_sys.h>

#include "config.hpp"

void EspApiImpl::pinMode(uint8_t pin, GpioMode mode) {
    int realMode = INPUT;
    switch (mode) {
    case GpioMode::input:
        realMode = INPUT;
        break;
    case GpioMode::inputPullup:
        realMode = INPUT_PULLUP;
        break;
    case GpioMode::output:
        realMode = OUTPUT;
        break;
    }
    ::pinMode(pin, realMode);
}

void EspApiImpl::digitalWrite(uint8_t pin, uint8_t val) {
    ::digitalWrite(pin, val);
}

int EspApiImpl::digitalRead(uint8_t pin) {
    return ::digitalRead(pin);
}

unsigned long EspApiImpl::millis() {
    return ::millis();
}

void EspApiImpl::delay(unsigned long ms) {
    ::delay(ms);
}

void EspApiImpl::restart(bool hard) {
    if (hard) {
        if (deviceConfig.resetPin <= 16) {
            pinMode(deviceConfig.resetPin, GpioMode::output);
            digitalWrite(deviceConfig.resetPin, 0);
            delay(10000);
        }
        ESP.reset();
    } else {
        ESP.restart();
    }
}

uint32_t EspApiImpl::getFreeHeap() {
    return ESP.getFreeHeap();
}

void EspApiImpl::attachInterrupt(
    uint8_t pin, std::function<void(void)> intRoutine, InterruptMode mode) {
    int realMode = CHANGE;
    switch (mode) {
    case InterruptMode::rise:
        realMode = RISING;
        break;
    case InterruptMode::fall:
        realMode = FALLING;
        break;
    case InterruptMode::change:
        realMode = CHANGE;
        break;
    }
    return ::attachInterrupt(pin, intRoutine, realMode);
}

void EspApiImpl::doDisableInterrupt() {
    ETS_GPIO_INTR_DISABLE();
}

void EspApiImpl::doEnableInterrupt() {
    ETS_GPIO_INTR_ENABLE();
}
