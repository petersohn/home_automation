#ifndef COUNTERINTERFACE_HPP
#define COUNTERINTERFACE_HPP

#include "SensorInterface.hpp"
#include "common/Interface.hpp"

class CounterInterface : public Interface {
public:
    CounterInterface(int pin, int bounceTime, float multiplier, int interval,
            int offset);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    class CounterSensor;

    std::unique_ptr<CounterSensor> createCounterSensor(float multiplier);
    void onRise();

    CounterSensor* counterSensor  = nullptr;
    int bounceTime;
    SensorInterface sensorInterface;
    volatile int riseCount = 0;
    volatile long lastRise = 0;
};

#endif // COUNTERINTERFACE_HPP

