#include "EspAnalogInput.hpp"

#include <Arduino.h>

int EspAnalogInput::read(std::uint8_t channel) {
    return analogRead(channel);
}

int EspAnalogInput::getMaxValue() const {
    return 1024;
}
