#ifndef POWER_SUPPLY_CONFIG_HPP
#define POWER_SUPPLY_CONFIG_HPP

#include <cstdint>
#include <string>

struct PowerSupplyConfig {
    uint8_t powerSwitchPin = 0;
    uint8_t resetSwitchPin = 0;
    uint8_t powerCheckPin = 0;
    unsigned pushTime = 200;
    unsigned forceOffTime = 6000;
    unsigned checkTime = 60000;
    std::string initialState;
};

#endif  // POWER_SUPPLY_CONFIG_HPP