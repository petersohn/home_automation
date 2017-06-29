#include "DallasTemperatureSensor.hpp"

#include "debug.hpp"

DallasTemperatureSensor::DallasTemperatureSensor(int pin)
        : oneWire(pin), sensor(&oneWire) {
    sensor.begin();
    ok = sensor.getAddress(address, 0);
    if (!ok) {
        DEBUGLN("Failed to initialize temperature sensor.");
        return;
    }

    sensor.setResolution(address, 9);
}

std::vector<String> DallasTemperatureSensor::measure() {
    sensor.requestTemperatures();
    float temperature = sensor.getTempC(address);
    if (temperature == DEVICE_DISCONNECTED_C) {
        DEBUGLN("Failed to read temperature.");
        return {};
    }

    return {String{temperature}};
}
