#ifndef HM3301SENSOR_HPP
#define HM3301SENSOR_HPP

#include "common/Sensor.hpp"

#include <Seeed_HM330X.h>

class HM3301Sensor : public Sensor {
public:
    bool initialized = false;

    HM3301Sensor(int sda, int sdl);

    std::vector<std::string> measure() override;
private:
    HM330X sensor;

    bool initialize();
};


#endif // HM3301SENSOR_HPP
