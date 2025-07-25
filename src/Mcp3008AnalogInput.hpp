#ifndef MCP3008_ANALOG_INPUT_HPP
#define MCP3008_ANALOG_INPUT_HPP

#include <memory>

#include "common/AnalogInput.hpp"

class Adafruit_MCP3008;

class Mcp3008AnalogInput : public AnalogInput {
public:
    Mcp3008AnalogInput(uint8_t sck, uint8_t mosi, uint8_t miso, uint8_t cs);
    virtual ~Mcp3008AnalogInput() override;
    virtual int read(std::uint8_t channel) override;
    virtual int getMaxValue() const override;

private:
    std::unique_ptr<Adafruit_MCP3008> impl;
};

#endif  // MCP3008_ANALOG_INPUT_HPP
