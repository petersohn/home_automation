#ifndef FAKE_ANALOG_INPUT_HPP
#define FAKE_ANALOG_INPUT_HPP

#include <vector>

#include "common/AnalogInput.hpp"

class FakeAnalogInput : public AnalogInput {
public:
    std::vector<int> values;
    int maxValue = 1024;

    virtual int read(std::uint8_t channel) override;
    virtual int getMaxValue() const override;
};

#endif  // FAKE_ANALOG_INPUT_HPP
