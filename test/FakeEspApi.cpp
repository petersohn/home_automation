#include "FakeEspApi.hpp"

void FakeEspApi::pinMode(uint8_t /* pin */, GpioMode /* mode */) {
}

void FakeEspApi::digitalWrite(uint8_t pin, uint8_t val) {
    pinValues[pin] = val != 0;
}

int FakeEspApi::digitalRead(uint8_t pin) {
    auto it = pinValues.find(pin);
    return it != pinValues.end() ? it->second : 0;
}

int FakeEspApi::analogRead(uint8_t /* pin */) {
    return 0;
}

unsigned long FakeEspApi::millis() {
    return time;
}

void FakeEspApi::delay(unsigned long ms) {
    time += ms;
}

void FakeEspApi::restart(bool /*hard*/) {
    restarted = true;
    time = 0;
}


uint32_t FakeEspApi::getFreeHeap() {
    return 0;
}


void FakeEspApi::attachInterrupt(uint8_t /* pin */,
    std::function<void(void)> /* intRoutine */, InterruptMode /* mode */) {
}

void FakeEspApi::doDisableInterrupt() {
}

void FakeEspApi::doEnableInterrupt() {
}

