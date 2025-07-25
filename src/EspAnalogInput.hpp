#ifndef ESP_ANALOG_INPUT_HPP
#define ESP_ANALOG_INPUT_HPP

#include "common/AnalogInput.hpp"

class EspAnalogInput : public AnalogInput {
public:
    virtual int read(std::uint8_t channel) override;
    virtual int getMaxValue() const override;
};

#endif  // ESP_ANALOG_INPUT_HPP
