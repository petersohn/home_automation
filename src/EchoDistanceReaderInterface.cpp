#include "EchoDistanceReaderInterface.hpp"

#include <Arduino.h>

#include "tools/string.hpp"

extern "C" {
#include "c_types.h"
}

constexpr double speedOfSound = 0.00034; // m/us
constexpr unsigned long timeout = 150000; // us

EchoDistanceReaderInterface::EchoDistanceReaderInterface(std::ostream& debug,
         EspApi& esp, uint8_t echoPin)
    : debug(debug)
    , esp(esp)
    , echoPin(echoPin) {
    esp.pinMode(echoPin, GpioMode::input);
    attachInterruptArg(echoPin, onChangeStatic, this, CHANGE);
}

void EchoDistanceReaderInterface::start() {
}

void EchoDistanceReaderInterface::execute(const std::string& /*command*/) {
}

void EchoDistanceReaderInterface::update(Actions action) {
    auto now = micros();
    if (now < riseTime) {
        debug << "Microseconds overflow." << std::endl;
        reset();
        return;
    }

    if (riseTime == 0) {
        return;
    }

    if (fallTime == 0) {
        if (now > riseTime + timeout) {
            debug << "Measurement timeout." << std::endl;
            reset();
        }
        return;
    }

    double distance = (fallTime - riseTime) * speedOfSound / 2.0;
    action.fire(std::vector<std::string>{tools::floatToString(distance, 3)});
    reset();
}

void EchoDistanceReaderInterface::reset() {
    riseTime = 0;
    fallTime = 0;
}

void IRAM_ATTR EchoDistanceReaderInterface::onChange() {
    auto now = micros();
    auto value = esp.digitalRead(echoPin);

    if (value) {
        if (riseTime == 0) {
            riseTime = now;
        }
    } else {
        if (riseTime != 0 && fallTime == 0) {
            fallTime = now;
        }

    }
}

void IRAM_ATTR EchoDistanceReaderInterface::onChangeStatic(void* arg) {
    static_cast<EchoDistanceReaderInterface*>(arg)->onChange();
}
