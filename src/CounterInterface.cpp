#include "CounterInterface.hpp"

#include "common/Sensor.hpp"
#include "debug.hpp"

#include <Arduino.h>
#include <FunctionalInterrupt.h>

class CounterInterface::CounterSensor : public Sensor {
public:
    CounterSensor(float multiplier) : multiplier(multiplier) {}

    std::vector<std::string> measure() override {
        if (lastMeasurement == 0) {
            lastMeasurement = millis();
            number = 0;
            return {};
        }

        auto now = millis();
        auto timeDifference = now - lastMeasurement;
        // mesure in ticks per second
        float result = number * 1000.0f / timeDifference * multiplier;
        number = 0;
        lastMeasurement = now;
        return {std::to_string(result)};
    }

    void increment(int n) { number += n; }
private:
    float multiplier;
    unsigned number = 0;
    unsigned long lastMeasurement = 0;
};

CounterInterface::CounterInterface(
        int pin, int bounceTime, float multiplier, int interval, int offset)
        : bounceTime(bounceTime),
          sensorInterface(createCounterSensor(multiplier), interval, offset) {
    pinMode(pin, INPUT);
    attachInterrupt(pin, [this]() { onRise(); }, RISING);
}

void CounterInterface::onRise() {
    long now = millis();
    if (now - lastRise > bounceTime) {
        ++riseCount;
        lastRise = now;
    }
}

auto CounterInterface::createCounterSensor(
        float multiplier) -> std::unique_ptr<CounterSensor> {
    std::unique_ptr<CounterSensor> result{new CounterSensor{multiplier}};
    counterSensor = result.get();
    return result;
}

void CounterInterface::start() {
    sensorInterface.start();
}

void CounterInterface::execute(const std::string& command) {
    sensorInterface.execute(command);
}

void CounterInterface::update(Actions action) {
    while (riseCount > 0) {
        int count = riseCount;
        counterSensor->increment(count);
        riseCount -= count;
    }
    sensorInterface.update(action);
}
