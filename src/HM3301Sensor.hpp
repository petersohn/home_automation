#ifndef HM3301SENSOR_HPP
#define HM3301SENSOR_HPP

#include "common/Sensor.hpp"

#include <Seeed_HM330X.h>

#include <ostream>

class HM3301Sensor : public Sensor {
public:
    bool initialized = false;

    HM3301Sensor(std::ostream& debug, int sda, int sdl);

    std::optional<std::vector<std::string>> measure() override;
private:
    std::ostream& debug;
    HM330X sensor;

    bool initialize();
};


#endif // HM3301SENSOR_HPP
