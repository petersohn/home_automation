#ifndef DHTSENSOR_HPP
#define DHTSENSOR_HPP

#include "Sensor.hpp"

#include <DHT.h>

class DhtSensor : public Sensor{
public:
    DhtSensor(int pin, int type) : dht(pin, type) {}

    std::vector<String> measure() override;
private:
    DHT dht;
};


#endif // DHTSENSOR_HPP
