#ifndef DALLASTEMPERATURESENSOR_HPP
#define DALLASTEMPERATURESENSOR_HPP

#include "common/Sensor.hpp"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <array>
#include <vector>

class DallasTemperatureSensor : public Sensor {
public:
    DallasTemperatureSensor(int pin);

    std::vector<std::string> measure() override;
private:
    OneWire oneWire;
    DallasTemperature sensors;
    bool initialized = false;

    std::vector<std::array<std::uint8_t, 8>> addresses;

    void initialize();
};

#endif // DALLASTEMPERATURESENSOR_HPP
