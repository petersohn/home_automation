#ifndef PWM_CONFIG_HPP
#define PWM_CONFIG_HPP

#include <cstdint>

struct PwmConfig {
    uint8_t pin = 0;
    int defaultValue = 0;
    bool invert = false;
};

#endif  // PWM_CONFIG_HPP