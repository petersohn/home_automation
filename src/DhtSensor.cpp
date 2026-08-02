#include "DhtSensor.hpp"

#include <cmath>

#include "tools/string.hpp"

namespace {

bool isOk(float value) {
    return value != 0.0f && !std::isnan(value);
}

}  // unnamed namespace

DhtSensor::DhtSensor(std::ostream& debug, const DhtConfig& config)
    : debug(debug), dht(config.pin, config.type) {
    this->dht.begin();
}

std::optional<std::vector<std::string>> DhtSensor::measure() {
    float temperature = this->dht.readTemperature();
    if (!isOk(temperature)) {
        this->debug << "temperature fail" << std::endl;
        return std::vector<std::string>{};
    }
    float humidity = this->dht.readHumidity();
    if (!isOk(humidity)) {
        this->debug << "humidity fail" << std::endl;
        return std::vector<std::string>{};
    }
    return std::vector<std::string>{
        tools::floatToString(temperature, 1),
        tools::floatToString(humidity, 1)};
}
