#ifndef ECHO_DISTANCE_SENSOR_HPP
#define ECHO_DISTANCE_SENSOR_HPP

#include <ostream>

#include "common/EspApi.hpp"
#include "common/Sensor.hpp"

class EchoDistanceSensor : public Sensor {
public:
    EchoDistanceSensor(
        std::ostream& debug, EspApi& esp, uint8_t triggerPin, uint8_t echoPin,
        unsigned triggerTime);

    std::optional<std::vector<std::string>> measure() override;

private:
    std::ostream& debug;
    EspApi& esp;

    uint8_t triggerPin;
    uint8_t echoPin;
    unsigned triggerTime;
    volatile unsigned long measurementStartTime = 0;
    volatile unsigned long riseTime = 0;
    volatile unsigned long echoTime = 0;

    void onChange();
    static void onChangeStatic(void* arg);
    void reset();
};

#endif  // ECHO_DISTANCE_SENSOR_HPP
