#ifndef HC_SR04SENSOR_HPP
#define HC_SR04SENSOR_HPP

#include "common/EspApi.hpp"
#include "common/Sensor.hpp"

#include <ostream>

class HC_SR04Sensor : public Sensor {
public:
    HC_SR04Sensor(std::ostream& debug, EspApi& esp, uint8_t triggerPin, uint8_t echoPin);

    std::optional<std::vector<std::string>> measure() override;

private:
    std::ostream& debug;
    EspApi& esp;

    uint8_t triggerPin;
    uint8_t echoPin;
    volatile unsigned long measurementStartTime = 0;
    volatile unsigned long riseTime = 0;
    volatile unsigned long echoTime = 0;

    void onChange();
    static void onChangeStatic(void* arg);
    void reset();
};


#endif // HC_SR04SENSOR_HPP
