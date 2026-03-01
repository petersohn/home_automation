#include "FakeAnalogInput.hpp"

int FakeAnalogInput::read(std::uint8_t channel) {
    return channel < this->values.size() ? this->values[channel] : 0;
}

int FakeAnalogInput::getMaxValue() const {
    return this->maxValue;
}
