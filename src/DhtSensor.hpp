#ifndef DHTSENSOR_HPP
#define DHTSENSOR_HPP

#include <DHT.h>

#include <ostream>

#include "common/DhtConfig.hpp"
#include "common/Sensor.hpp"

class DhtSensor : public Sensor {
public:
    DhtSensor(std::ostream& debug, const DhtConfig& config);

    std::optional<std::vector<std::string>> measure() override;

private:
    std::ostream& debug;
    DHT dht;
};

#endif  // DHTSENSOR_HPP
