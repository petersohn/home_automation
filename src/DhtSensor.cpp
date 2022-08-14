#include "DhtSensor.hpp"

#include "tools/string.hpp"

#include <cmath>

namespace {

bool isOk(float value) {
    return value != 0.0f && !std::isnan(value);
}

} // unnamed namespace

DhtSensor::DhtSensor(std::ostream& debug, uint8_t pin, int type)
    : debug(debug), dht(pin, type) {
    dht.begin();
}

std::vector<std::string> DhtSensor::measure() {
    float temperature = dht.readTemperature();
    if (!isOk(temperature)) {
        debug << "temperature fail" << std::endl;
        return {};
    }
    float humidity = dht.readHumidity();
    if (!isOk(humidity)) {
        debug << "humidity fail" << std::endl;
        return {};
    }
    return {tools::floatToString(temperature, 1), tools::floatToString(humidity, 1)};
}
