#ifndef COUNTERINTERFACE_HPP
#define COUNTERINTERFACE_HPP

#include "SensorInterface.hpp"
#include "common/Interface.hpp"

#include <Bounce2.h>

class CounterInterface : public Interface {
public:
    CounterInterface(int pin, float multiplier, int interval, int offset);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    class CounterSensor;

    std::unique_ptr<CounterSensor> createCounterSensor(float multiplier);

    Bounce bounce;
    CounterSensor* counterSensor  = nullptr;
    SensorInterface sensorInterface;
};

#endif // COUNTERINTERFACE_HPP

