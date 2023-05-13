#ifndef COUNTERINTERFACE_HPP
#define COUNTERINTERFACE_HPP

#include "SensorInterface.hpp"
#include "common/Interface.hpp"
#include "common/EspApi.hpp"

#include <ostream>

class CounterInterface : public Interface {
public:
    CounterInterface(std::ostream& debug, EspApi& esp, std::string name,
            uint8_t pin, int bounceTime, float multiplier, int interval,
            int offset, std::vector<std::string> pulse);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    class CounterSensor;

    std::unique_ptr<CounterSensor> createCounterSensor(float multiplier);
    void onRise();
    static void onRiseStatic(void* arg);
    void resetMinInterval();

    CounterSensor* counterSensor  = nullptr;
    int bounceTime;
    int interval;
    SensorInterface sensorInterface;
    volatile int riseCount = 0;
    volatile long lastRise = 0;
    volatile int minInterval;
};

#endif // COUNTERINTERFACE_HPP

