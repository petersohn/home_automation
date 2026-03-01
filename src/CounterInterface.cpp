#include "CounterInterface.hpp"

#include <Arduino.h>

#include "common/Sensor.hpp"
#include "tools/string.hpp"

extern "C" {
#include "c_types.h"
#include "ets_sys.h"
}

#include <memory>

class CounterInterface::CounterSensor : public Sensor {
public:
    CounterSensor(float multiplier) : multiplier(multiplier) {}

    std::optional<std::vector<std::string>> measure() override {
        auto now = millis();
        if (this->lastMeasurement == 0) {
            this->lastMeasurement = now;
            this->number = 0;
            return std::vector<std::string>{};
        }

        auto timeDifference = now - this->lastMeasurement;
        float averageResult = this->normalize(this->number, timeDifference);
        float maxResult = this->normalize(this->max, timeDifference);
        this->number = 0;
        this->max = 0;
        this->lastMeasurement = now;
        return std::vector<std::string>{
            tools::floatToString(averageResult, 4),
            tools::floatToString(maxResult, 4)};
    }

    void update(int counter, float maxRate) {
        this->number += counter;
        this->max = std::max(this->max, maxRate);
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

CounterInterface::CounterInterface(
    std::ostream& debug, EspApi& esp, std::string name, uint8_t pin,
    int bounceTime, float multiplier, int interval, int offset,
    std::vector<std::string> pulse)
    : bounceTime(bounceTime)
    , interval(interval)
    , sensorInterface(
          debug, esp, createCounterSensor(multiplier), std::move(name),
          interval, offset, std::move(pulse)) {
    pinMode(pin, INPUT);
    this->resetMinInterval();
    attachInterruptArg(pin, onRiseStatic, this, RISING);
}

void IRAM_ATTR CounterInterface::onRise() {
    long now = millis();

    int difference = now - this->lastRise;
    if (difference > this->bounceTime) {
        ++this->riseCount;
        this->minInterval = std::max(
            1, std::min(static_cast<int>(this->minInterval), difference));
        this->lastRise = now;
    }
}

auto CounterInterface::createCounterSensor(float multiplier)
    -> std::unique_ptr<CounterSensor> {
    auto result = std::make_unique<CounterSensor>(multiplier);
    this->counterSensor = result.get();
    return result;
}

void CounterInterface::start() {
    this->sensorInterface.start();
}

void CounterInterface::execute(const std::string& command) {
    this->sensorInterface.execute(command);
}

void CounterInterface::update(Actions action) {
    while (this->riseCount > 0) {
        int count = this->riseCount;
        float rate =
            (this->minInterval == this->interval)
                ? 0
                : static_cast<float>(this->interval) / this->minInterval;
        this->resetMinInterval();
        this->counterSensor->update(count, rate);
        this->riseCount -= count;
    }
    this->sensorInterface.update(action);
}

void CounterInterface::resetMinInterval() {
    this->minInterval = this->interval;
}
