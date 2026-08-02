#ifndef HM3301SENSOR_HPP
#define HM3301SENSOR_HPP

#include <Seeed_HM330X.h>

#include <ostream>

#include "common/Hm3301Config.hpp"
#include "common/Sensor.hpp"

class HM3301Sensor : public Sensor {
public:
    bool initialized = false;

    HM3301Sensor(std::ostream& debug, const Hm3301Config& config);

    std::optional<std::vector<std::string>> measure() override;

private:
    std::ostream& debug;
    HM330X sensor;

    bool initialize();
};

#endif  // HM3301SENSOR_HPP
