#include "EchoDistanceSensor.hpp"

#include <Arduino.h>

#include "tools/string.hpp"

extern "C" {
#include "c_types.h"
}

constexpr double speedOfSound = 0.00034;  // m/us
constexpr unsigned long timeout = 150;    // ms

EchoDistanceSensor::EchoDistanceSensor(
    std::ostream& debug, EspApi& esp, uint8_t triggerPin, uint8_t echoPin,
    unsigned triggerTime)
    : debug(debug)
    , esp(esp)
    , triggerPin(triggerPin)
    , echoPin(echoPin)
    , triggerTime(triggerTime) {
    esp.pinMode(triggerPin, GpioMode::output);
    esp.pinMode(echoPin, GpioMode::input);
    esp.digitalWrite(triggerPin, 0);
    attachInterruptArg(echoPin, onChangeStatic, this, CHANGE);
}

std::optional<std::vector<std::string>> EchoDistanceSensor::measure() {
    if (this->measurementStartTime == 0) {
        this->measurementStartTime = this->esp.millis();
        this->esp.digitalWrite(this->triggerPin, 1);
        delayMicroseconds(triggerTime);
        this->esp.digitalWrite(this->triggerPin, 0);
        return std::nullopt;
    }

    bool error = false;

    if (this->esp.millis() - this->measurementStartTime > timeout) {
        this->debug << "Measurement timeout." << std::endl;
        reset();
        return std::vector<std::string>{};
    }

    if (this->echoTime == 0) {
        if (this->riseTime != 0) {
            auto now = micros();
            if (now < this->riseTime) {
                this->debug << "Microseconds overflow." << std::endl;
                reset();
                return std::vector<std::string>{};
            }
        }
        return std::nullopt;
    }

    auto measuredTime = this->echoTime;
    reset();

    double distance = measuredTime * speedOfSound / 2.0;
    return std::vector<std::string>{tools::floatToString(distance, 3)};
}

void EchoDistanceSensor::reset() {
    this->measurementStartTime = 0;
    this->riseTime = 0;
    this->echoTime = 0;
}

void IRAM_ATTR EchoDistanceSensor::onChange() {
    auto value = this->esp.digitalRead(this->echoPin);

    if (value) {
        if (this->measurementStartTime == 0) {
            return;
        }

        this->riseTime = micros();
    } else {
        if (this->riseTime == 0) {
            return;
        }

        this->echoTime = micros() - this->riseTime;
        this->riseTime = 0;
    }
}

void IRAM_ATTR EchoDistanceSensor::onChangeStatic(void* arg) {
    static_cast<EchoDistanceSensor*>(arg)->onChange();
}
