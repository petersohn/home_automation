#include "DallasTemperatureSensor.hpp"
#include "tools/string.hpp"

DallasTemperatureSensor::DallasTemperatureSensor(
    std::ostream& debug, uint8_t pin, std::size_t expectedNumberOfDevices)
    : debug(debug)
    , oneWire(pin)
    , sensors(&oneWire)
    , expectedNumberOfDevices(expectedNumberOfDevices) {
    initialize();
}

std::optional<std::vector<std::string>> DallasTemperatureSensor::measure() {
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
            debug << "Failed to read temperature." << std::endl;
            initialized = false;
            return std::vector<std::string>{};
        }
        auto value = tools::floatToString(temperature, 1);
        result.emplace_back(value);
    }
    return result;
}

bool DallasTemperatureSensor::initialize() {
    sensors.begin();
    int count = sensors.getDeviceCount();
    debug << "Number of devices: " << count
          << ", expected: " << expectedNumberOfDevices << std::endl;
    addresses.clear();
    addresses.reserve(count);
    for (int i = 0; i < count; ++i) {
        addresses.emplace_back();
        if (!sensors.getAddress(addresses.back().data(), i)) {
            addresses.pop_back();
            debug << "Failed to initialize temperature sensor at index " << i
                  << std::endl;
            continue;
        }
        sensors.setResolution(addresses.back().data(), 9);
    }
    initialized = addresses.size() == expectedNumberOfDevices;
    return initialized;
}
