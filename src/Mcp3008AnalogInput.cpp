#include "Mcp3008AnalogInput.hpp"

#include <Adafruit_MCP3008.h>

Mcp3008AnalogInput::Mcp3008AnalogInput(
    uint8_t sck, uint8_t mosi, uint8_t miso, uint8_t cs)
    : impl(std::make_unique<Adafruit_MCP3008>()) {
    impl->begin(sck, mosi, miso, cs);
}

Mcp3008AnalogInput::~Mcp3008AnalogInput() {}

int Mcp3008AnalogInput::read(std::uint8_t channel) {
    return impl->readADC(channel);
}

int Mcp3008AnalogInput::getMaxValue() const {
    return 1024;
}
