#ifndef KEEPALIVE_CONFIG_HPP
#define KEEPALIVE_CONFIG_HPP

#include <cstdint>

struct KeepaliveConfig {
    uint8_t pin = 0;
    unsigned interval = 10000;
    unsigned resetInterval = 10;
};

#endif  // KEEPALIVE_CONFIG_HPP