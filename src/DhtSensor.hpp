#ifndef DHTSENSOR_HPP
#define DHTSENSOR_HPP

#include "common/Sensor.hpp"

#include <DHT.h>

class DhtSensor : public Sensor {
public:
    DhtSensor(uint8_t pin, int type);

    std::vector<std::string> measure() override;
private:
    DHT dht;
};


#endif // DHTSENSOR_HPP
