#ifndef ENCODER_CONFIG_HPP
#define ENCODER_CONFIG_HPP

#include <cstdint>

struct EncoderConfig {
    uint8_t downPin = 0;
    uint8_t upPin = 0;
    bool pulse = false;
};

#endif  // ENCODER_CONFIG_HPP