#ifndef COVER_CONFIG_HPP
#define COVER_CONFIG_HPP

#include <cstdint>
#include <vector>

struct PositionSensor {
    int position = 0;
    uint8_t pin = 0;
    bool invert = false;
};

struct CoverConfig {
    uint8_t upMovementPin = 0;
    uint8_t downMovementPin = 0;
    uint8_t upPin = 0;
    uint8_t downPin = 0;
    uint8_t stopPin = 0;
    bool latching = false;
    bool invertInput = false;
    bool invertOutput = false;
    int closedPosition = 0;
    bool invertPositionSensors = false;
    std::vector<PositionSensor> positionSensors;
};

#endif  // COVER_CONFIG_HPP