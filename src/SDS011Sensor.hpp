#ifndef SDS011SENSOR_HPP
#define SDS011SENSOR_HPP

#include "common/Sensor.hpp"
#include "SDS011/SDS011.h"

#include <ostream>

class SDS011Sensor : public Sensor {
public:
    SDS011Sensor(std::ostream& debug, uint8_t rx, uint8_t tx, uint16_t meaureTime);

    virtual std::vector<std::string> measure() override;
private:
    std::ostream& debug;
    SDS011 sds;
    const uint16_t measureTime;

    bool isAwake;
    uint16_t failCount;
    uint16_t successCount;
};

#endif // SDS011SENSOR_HPP
