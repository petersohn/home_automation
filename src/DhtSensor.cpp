#include "DhtSensor.hpp"
#include "debug.hpp"

#include "tools/string.hpp"

#include <cmath>

namespace {

bool isOk(float value) {
    return value != 0.0f && !std::isnan(value);
}

} // unnamed namespace

DhtSensor::DhtSensor(uint8_t pin, int type) : dht(pin, type) {
	dht.begin();
}

std::vector<std::string> DhtSensor::measure() {
    float temperature = dht.readTemperature();
    if (!isOk(temperature)) {
    	debugln("temperature fail");
        return {};
    }
    float humidity = dht.readHumidity();
    if (!isOk(humidity)) {
    	debugln("humidity fail");
        return {};
    }
    return {tools::floatToString(temperature, 1), tools::floatToString(humidity, 1)};
}
