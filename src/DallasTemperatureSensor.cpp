#include "DallasTemperatureSensor.hpp"

#include "tools/string.hpp"

DallasTemperatureSensor::DallasTemperatureSensor(
    std::ostream& debug, uint8_t pin, std::size_t expectedNumberOfDevices)
    : debug(debug)
    , oneWire(pin)
    , sensors(&oneWire)
    , expectedNumberOfDevices(expectedNumberOfDevices) {
    this->initialize();
}

std::optional<std::vector<std::string>> DallasTemperatureSensor::measure() {
    if (!this->initialized) {
        if (!this->initialize()) {
            return {};
        }
    }
    this->sensors.requestTemperatures();
    std::vector<std::string> result;
    for (const auto& address : this->addresses) {
        float temperature = this->sensors.getTempC(address.data());
        if (temperature == DEVICE_DISCONNECTED_C) {
            this->debug << "Failed to read temperature." << std::endl;
            this->initialized = false;
            return std::vector<std::string>{};
        }
        auto value = tools::floatToString(temperature, 1);
        result.emplace_back(value);
    }
    return result;
}

bool DallasTemperatureSensor::initialize() {
    this->sensors.begin();
    int count = this->sensors.getDeviceCount();
    this->debug << "Number of devices: " << count
                << ", expected: " << this->expectedNumberOfDevices << std::endl;
    this->addresses.clear();
    this->addresses.reserve(count);
    for (int i = 0; i < count; ++i) {
        this->addresses.emplace_back();
        if (!this->sensors.getAddress(this->addresses.back().data(), i)) {
            this->addresses.pop_back();
            this->debug << "Failed to initialize temperature sensor at index "
                        << i << std::endl;
            continue;
        }
        this->sensors.setResolution(this->addresses.back().data(), 9);
    }
    this->initialized = this->addresses.size() == this->expectedNumberOfDevices;
    return this->initialized;
}
