#include "SensorInterface.hpp"

#include "debug.hpp"

#include <Arduino.h>

void SensorInterface::start() {
    // When connected to the network, all sensors make a measurement.
    // Afterwards, measurements are shifted by offset.
    nextExecution = millis() - offset;
}

void SensorInterface::execute(const std::string& /*command*/) {
}

void SensorInterface::update(Actions action) {
    long now = millis();
    if ((nextExecution != 0 && now >= nextExecution)
            || (nextRetry != 0 && now >= nextRetry)) {
        if (now >= nextExecution) {
            nextExecution += ((now - nextExecution) / interval + 1) * interval;
        }
        auto values = sensor->measure();
        if (values.empty()) {
            debugln("Measurement failed. Trying again.");
            nextRetry = now + 1000;
        } else {
            nextRetry = 0;
            action.fire(values);
        }
    }
}
