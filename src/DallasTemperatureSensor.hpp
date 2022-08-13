#ifndef DALLASTEMPERATURESENSOR_HPP
#define DALLASTEMPERATURESENSOR_HPP

#include "common/Sensor.hpp"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <array>
#include <vector>

class DallasTemperatureSensor : public Sensor {
public:
    DallasTemperatureSensor(uint8_t pin, std::size_t expectedNumberOfDevices);

    std::vector<std::string> measure() override;
private:
    OneWire oneWire;
    DallasTemperature sensors;
    std::size_t expectedNumberOfDevices;
    bool initialized = false;

    std::vector<std::array<std::uint8_t, 8>> addresses;

    bool initialize();
};

#endif // DALLASTEMPERATURESENSOR_HPP
