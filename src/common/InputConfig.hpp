#ifndef INPUT_CONFIG_HPP
#define INPUT_CONFIG_HPP

#include <cstdint>

enum class CycleType { none, single, multi };

struct InputConfig {
    uint8_t pin = 0;
    CycleType cycleType = CycleType::single;
    unsigned debounce = 10;
};

#endif  // INPUT_CONFIG_HPP