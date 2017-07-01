#ifndef COUNTERINTERFACE_HPP
#define COUNTERINTERFACE_HPP

#include "Interface.hpp"
#include "Sensor.hpp"
#include "SensorInterface.hpp"

#include <Bounce2.h>

class CounterInterface : public Interface {
public:
    CounterInterface(int pin, float multiplier, int interval, int offset);
    void execute(const String& command) override;
    void update(Actions action) override;

private:
    class CounterSensor;

    std::unique_ptr<CounterSensor> createCounterSensor(float multiplier);

    Bounce bounce;
    CounterSensor* counterSensor  = nullptr;
    SensorInterface sensorInterface;
};

#endif // COUNTERINTERFACE_HPP

