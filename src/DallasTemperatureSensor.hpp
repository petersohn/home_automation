#ifndef DALLASTEMPERATURESENSOR_HPP
#define DALLASTEMPERATURESENSOR_HPP

#include "common/Sensor.hpp"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <array>
#include <vector>
#include <ostream>

class DallasTemperatureSensor : public Sensor {
public:
    DallasTemperatureSensor(std::ostream& debug,
            uint8_t pin, std::size_t expectedNumberOfDevices);

    std::optional<std::vector<std::string>> measure() override;
private:
    std::ostream& debug;

    OneWire oneWire;
    DallasTemperature sensors;
    std::size_t expectedNumberOfDevices;
    bool initialized = false;

    std::vector<std::array<std::uint8_t, 8>> addresses;

    bool initialize();
};

#endif // DALLASTEMPERATURESENSOR_HPP
