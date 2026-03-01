#ifndef ANALOG_INPUT_WITH_CHANNEL_HPP
#define ANALOG_INPUT_WITH_CHANNEL_HPP

#include <cstdint>
#include <memory>

#include "AnalogInput.hpp"

class AnalogInput;

class AnalogInputWithChannel {
public:
    AnalogInputWithChannel(std::shared_ptr<AnalogInput> input, uint8_t channel)
        : input(std::move(input)), channel(channel) {}

    int read() { return this->input->read(this->channel); }
    int getMaxValue() { return this->input->getMaxValue(); }

private:
    std::shared_ptr<AnalogInput> input;
    uint8_t channel;
};

#endif  // ANALOG_INPUT_WITH_CHANNEL_HPP
