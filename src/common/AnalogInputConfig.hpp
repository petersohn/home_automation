#ifndef ANALOG_INPUT_CONFIG_HPP
#define ANALOG_INPUT_CONFIG_HPP

#include <cstdint>
#include <string>
#include <variant>

struct Mcp3008Config {
    uint8_t sck = 0;
    uint8_t mosi = 0;
    uint8_t miso = 0;
    uint8_t cs = 0;
};

struct AnalogInputConfig {
    std::string name;
    std::variant<Mcp3008Config> input;
};

#endif  // ANALOG_INPUT_CONFIG_HPP