#include "EchoDistanceReaderInterface.hpp"

#include <Arduino.h>

#include "tools/string.hpp"

extern "C" {
#include "c_types.h"
}

constexpr double speedOfSound = 0.00034;   // m/us
constexpr unsigned long timeout = 150000;  // us

EchoDistanceReaderInterface::EchoDistanceReaderInterface(
    std::ostream& debug, EspApi& esp, uint8_t echoPin)
    : debug(debug), esp(esp), echoPin(echoPin) {
    esp.pinMode(echoPin, GpioMode::input);
    attachInterruptArg(echoPin, onChangeStatic, this, CHANGE);
}

void EchoDistanceReaderInterface::start() {}

void EchoDistanceReaderInterface::execute(const std::string& /*command*/) {}

void EchoDistanceReaderInterface::update(Actions action) {
    auto now = micros();
    if (now < this->riseTime) {
        this->debug << "Microseconds overflow." << std::endl;
        reset();
        return;
    }

    if (this->riseTime == 0) {
        return;
    }

    if (this->fallTime == 0) {
        if (now > this->riseTime + timeout) {
            this->debug << "Measurement timeout." << std::endl;
            reset();
        }
        return;
    }

    double distance = (this->fallTime - this->riseTime) * speedOfSound / 2.0;
    action.fire(std::vector<std::string>{tools::floatToString(distance, 3)});
    reset();
}

void EchoDistanceReaderInterface::reset() {
    this->riseTime = 0;
    this->fallTime = 0;
}

void IRAM_ATTR EchoDistanceReaderInterface::onChange() {
    auto now = micros();
    auto value = this->esp.digitalRead(this->echoPin);

    if (value) {
        if (this->riseTime == 0) {
            this->riseTime = now;
        }
    } else {
        if (this->riseTime != 0 && this->fallTime == 0) {
            this->fallTime = now;
        }
    }
}

void IRAM_ATTR EchoDistanceReaderInterface::onChangeStatic(void* arg) {
    static_cast<EchoDistanceReaderInterface*>(arg)->onChange();
}
