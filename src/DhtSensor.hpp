#ifndef DHTSENSOR_HPP
#define DHTSENSOR_HPP

#include "common/Sensor.hpp"

#include <DHT.h>

class DhtSensor : public Sensor{
public:
    DhtSensor(int pin, int type) : dht(pin, type) {}

    std::vector<std::string> measure() override;
private:
    DHT dht;
};


#endif // DHTSENSOR_HPP
