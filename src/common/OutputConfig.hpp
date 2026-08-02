#ifndef OUTPUT_CONFIG_HPP
#define OUTPUT_CONFIG_HPP

#include <cstdint>

struct OutputConfig {
    uint8_t pin = 0;
    bool defaultValue = false;
    bool invert = false;
};

#endif  // OUTPUT_CONFIG_HPP