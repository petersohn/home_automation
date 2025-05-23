#include <Arduino.h>

#include "EchoDistanceSensor.hpp"
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
    if (measurementStartTime == 0) {
        measurementStartTime = esp.millis();
        esp.digitalWrite(triggerPin, 1);
        delayMicroseconds(triggerTime);
        esp.digitalWrite(triggerPin, 0);
        return std::nullopt;
    }

    bool error = false;

    if (esp.millis() - measurementStartTime > timeout) {
        debug << "Measurement timeout." << std::endl;
        reset();
        return std::vector<std::string>{};
    }

    if (echoTime == 0) {
        if (riseTime != 0) {
            auto now = micros();
            if (now < riseTime) {
                debug << "Microseconds overflow." << std::endl;
                reset();
                return std::vector<std::string>{};
            }
        }
        return std::nullopt;
    }

    auto measuredTime = echoTime;
    reset();

    double distance = measuredTime * speedOfSound / 2.0;
    return std::vector<std::string>{tools::floatToString(distance, 3)};
}

void EchoDistanceSensor::reset() {
    measurementStartTime = 0;
    riseTime = 0;
    echoTime = 0;
}

void IRAM_ATTR EchoDistanceSensor::onChange() {
    auto value = esp.digitalRead(echoPin);

    if (value) {
        if (measurementStartTime == 0) {
            return;
        }

        riseTime = micros();
    } else {
        if (riseTime == 0) {
            return;
        }

        echoTime = micros() - riseTime;
        riseTime = 0;
    }
}

void IRAM_ATTR EchoDistanceSensor::onChangeStatic(void* arg) {
    static_cast<EchoDistanceSensor*>(arg)->onChange();
}
