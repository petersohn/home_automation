#include "SensorInterface.hpp"

#include <Arduino.h>

void SensorInterface::execute(const String& /*command*/) {
}

void SensorInterface::update(Actions action) {
    auto now = millis();
    if (now >= nextExecution) {
        nextExecution += ((now - nextExecution) / interval + 1) * interval;
        action.fire(sensor->measure());
    }
}
