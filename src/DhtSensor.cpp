#include "DhtSensor.hpp"

#include <cmath>

namespace {

bool isOk(float value) {
    return value != 0.0f && !std::isnan(value);
}

} // unnamed namespace

std::vector<String> DhtSensor::measure() {
    float temperature = dht.readTemperature();
    if (!isOk(temperature)) {
        return {};
    }
    float humidity = dht.readHumidity();
    if (!isOk(humidity)) {
        return {};
    }
    return {String{temperature}, String{humidity}};
}
