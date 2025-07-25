#ifndef ANALOG_INPUT_HPP
#define ANALOG_INPUT_HPP

#include <cstdint>

class AnalogInput {
public:
    virtual int read(std::uint8_t channel) = 0;
    virtual int getMaxValue() const = 0;
    virtual ~AnalogInput() {}
};

#endif  // ANALOG_INPUT_HPP
