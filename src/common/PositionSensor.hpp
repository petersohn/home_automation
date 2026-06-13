#ifndef POSITION_SENSOR_HPP
#define POSITION_SENSOR_HPP

#include <cstdint>

struct PositionSensor {
    int position = 0;
    uint8_t pin = 0;
    bool invert = false;
};

#endif  // POSITION_SENSOR_HPP
