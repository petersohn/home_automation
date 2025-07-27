#include "FakeAnalogInput.hpp"

int FakeAnalogInput::read(std::uint8_t channel) {
    return channel < values.size() ? values[channel] : 0;
}

int FakeAnalogInput::getMaxValue() const {
    return maxValue;
}
