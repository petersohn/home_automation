#include "CounterInterface.hpp"

#include "common/Sensor.hpp"
#include "tools/string.hpp"

#include <Arduino.h>

extern "C" {
#include "c_types.h"
#include "ets_sys.h"
}

#include <memory>

class CounterInterface::CounterSensor : public Sensor {
public:
    CounterSensor(float multiplier)
        : multiplier(multiplier) {}

    std::vector<std::string> measure() override {
        auto now = millis();
        if (lastMeasurement == 0) {
            lastMeasurement = now;
            number = 0;
            return {};
        }

        auto timeDifference = now - lastMeasurement;
        float averageResult = normalize(number, timeDifference);
        float maxResult = normalize(max, timeDifference);
        number = 0;
        max = 0;
        lastMeasurement = now;
        return {tools::floatToString(averageResult, 4), tools::floatToString(maxResult, 4)};
    }

    void update(int counter, float maxRate) {
        number += counter;
        max = std::max(max, maxRate);
    }
private:
    float multiplier;
    unsigned number = 0;
    float max = 0;
    unsigned long lastMeasurement = 0;

    float normalize(float number, float timeDifference) {
        // mesure in ticks per second
        return number * 1000.0f / timeDifference * multiplier;
    }
};

void IRAM_ATTR CounterInterface::onRiseStatic(void* arg) {
    reinterpret_cast<CounterInterface*>(arg)->onRise();
}

CounterInterface::CounterInterface(std::ostream& debug, EspApi& esp,
        std::string name, uint8_t pin, int bounceTime, float multiplier,
        int interval, int offset, std::vector<std::string> pulse)
        : bounceTime(bounceTime),
          interval(interval),
          sensorInterface(debug, esp, createCounterSensor(multiplier), std::move(name),
          interval, offset, std::move(pulse)) {
    pinMode(pin, INPUT);
    resetMinInterval();
    attachInterruptArg(pin, onRiseStatic, this, RISING);
}

void IRAM_ATTR CounterInterface::onRise() {
    long now = millis();

    int difference = now - lastRise;
    if (difference > bounceTime) {
        ++riseCount;
        minInterval = std::max(1,
                std::min(static_cast<int>(minInterval), difference));
        lastRise = now;
    }
}

auto CounterInterface::createCounterSensor(float multiplier)
        -> std::unique_ptr<CounterSensor> {
    auto result = std::make_unique<CounterSensor>(multiplier);
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
        float rate = (minInterval == interval)
                ? 0 : static_cast<float>(interval) / minInterval;
        resetMinInterval();
        counterSensor->update(count, rate);
        riseCount -= count;
    }
    sensorInterface.update(action);
}

void CounterInterface::resetMinInterval() {
    minInterval = interval;
}
