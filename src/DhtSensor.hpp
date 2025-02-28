#ifndef DHTSENSOR_HPP
#define DHTSENSOR_HPP

#include "common/Sensor.hpp"

#include <ostream>

#include <DHT.h>

class DhtSensor : public Sensor {
public:
    DhtSensor(std::ostream& debug, uint8_t pin, int type);

    std::optional<std::vector<std::string>> measure() override;
private:
    std::ostream& debug;
    DHT dht;
};


#endif // DHTSENSOR_HPP
