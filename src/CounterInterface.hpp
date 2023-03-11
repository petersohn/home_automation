#ifndef COUNTERINTERFACE_HPP
#define COUNTERINTERFACE_HPP

#include "SensorInterface.hpp"
#include "common/Interface.hpp"
#include "common/EspApi.hpp"

#include <Bounce2.h>

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

    std::unique_ptr<CounterSensor> createCounterSensor(EspApi& esp, float multiplier);

    Bounce bounce;
    CounterSensor* counterSensor  = nullptr;
    int interval;
    SensorInterface sensorInterface;
};

#endif // COUNTERINTERFACE_HPP

