#include "DhtSensor.hpp"

std::vector<String> DhtSensor::measure() {
    return {String{dht.readTemperature()}, String{dht.readHumidity()}};
}
