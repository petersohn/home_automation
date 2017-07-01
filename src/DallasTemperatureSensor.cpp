#include "DallasTemperatureSensor.hpp"

#include "debug.hpp"

DallasTemperatureSensor::DallasTemperatureSensor(int pin)
        : oneWire(pin), sensors(&oneWire) {
    sensors.begin();
    int count = sensors.getDeviceCount();
    DEBUG("Number of devices: ");
    DEBUGLN(count);
    for (int i = 0; i < count; ++i) {
        addresses.emplace_back();
        if (!sensors.getAddress(addresses.back().data(), i)) {
            addresses.pop_back();
            DEBUG("Failed to initialize temperature sensor at index ");
            DEBUGLN(i);
            continue;
        }
        sensors.setResolution(addresses.back().data(), 9);
    }

}

std::vector<String> DallasTemperatureSensor::measure() {
    sensors.requestTemperatures();
    std::vector<String> result;
    for (const auto& address : addresses) {
        float temperature = sensors.getTempC(address.data());
        if (temperature == DEVICE_DISCONNECTED_C) {
            DEBUGLN("Failed to read temperature.");
            continue;
        }
        result.emplace_back(temperature);
    }
    return result;
}
