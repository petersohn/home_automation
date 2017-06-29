#ifndef DALLASTEMPERATURESENSOR_HPP
#define DALLASTEMPERATURESENSOR_HPP

#include "Sensor.hpp"

#include <OneWire.h>
#include <DallasTemperature.h>

class DallasTemperatureSensor : public Sensor {
public:
    DallasTemperatureSensor(int pin);

    std::vector<String> measure() override;
private:
    OneWire oneWire;
    DallasTemperature sensor;

    DeviceAddress address;
    bool ok;
};

#endif // DALLASTEMPERATURESENSOR_HPP
