#include "FakeEspApi.hpp"

#include <boost/test/unit_test_log.hpp>

void FakeEspApi::pinMode(uint8_t /* pin */, GpioMode /* mode */) {}

void FakeEspApi::digitalWrite(uint8_t pin, uint8_t val) {
    const bool value = val != 0;
    if (value != this->pinValues[pin]) {
        BOOST_TEST_MESSAGE(
            "Pin " << static_cast<int>(pin)
                   << " value=" << static_cast<int>(val));
        this->pinValues[pin] = value;
    }
}

int FakeEspApi::digitalRead(uint8_t pin) {
    auto it = this->pinValues.find(pin);
    return it != this->pinValues.end() ? it->second : 0;
}

unsigned long FakeEspApi::millis() {
    return this->time;
}

unsigned long FakeEspApi::micros() {
    return this->time * 1000;
}

void FakeEspApi::delay(unsigned long ms) {
    this->time += ms;
}

void FakeEspApi::restart(bool /*hard*/) {
    BOOST_TEST_MESSAGE("REBOOT");
    this->restarted = true;
    this->time = 0;
}

uint32_t FakeEspApi::getFreeHeap() {
    return 0;
}

void FakeEspApi::doDisableInterrupt() {}

void FakeEspApi::doEnableInterrupt() {}
