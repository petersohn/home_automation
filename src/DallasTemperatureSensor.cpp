#include "DallasTemperatureSensor.hpp"

#include "debug.hpp"
#include "tools/string.hpp"

DallasTemperatureSensor::DallasTemperatureSensor(int pin,
        std::size_t expectedNumberOfDevices)
        : oneWire(pin), sensors(&oneWire),
          expectedNumberOfDevices(expectedNumberOfDevices) {
    initialize();
}

std::vector<std::string> DallasTemperatureSensor::measure() {
    if (!initialized) {
        if (!initialize()) {
            return {};
        }
    }
    sensors.requestTemperatures();
    std::vector<std::string> result;
    for (const auto& address : addresses) {
        float temperature = sensors.getTempC(address.data());
        if (temperature == DEVICE_DISCONNECTED_C) {
            debugln("Failed to read temperature.");
            initialized = false;
            return {};
        }
        auto value = tools::floatToString(temperature, 1);
        result.emplace_back(value);
    }
    return result;
}

bool DallasTemperatureSensor::initialize() {
    sensors.begin();
    int count = sensors.getDeviceCount();
    debug("Number of devices: ");
    debug(count);
    debug(", expected: ");
    debugln(expectedNumberOfDevices);
    addresses.clear();
    addresses.reserve(count);
    for (int i = 0; i < count; ++i) {
        addresses.emplace_back();
        if (!sensors.getAddress(addresses.back().data(), i)) {
            addresses.pop_back();
            debug("Failed to initialize temperature sensor at index ");
            debugln(i);
            continue;
        }
        sensors.setResolution(addresses.back().data(), 9);
    }
    initialized = addresses.size() == expectedNumberOfDevices;
    return initialized;
}
