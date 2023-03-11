#include "CounterInterface.hpp"

#include "common/Sensor.hpp"
#include "tools/string.hpp"

#include <Arduino.h>

#include <memory>

class CounterInterface::CounterSensor : public Sensor {
public:
    CounterSensor(EspApi& esp, float multiplier)
        : esp(esp), multiplier(multiplier) {}

    std::vector<std::string> measure() override {
        auto now = esp.millis();
        if (lastMeasurement == 0) {
            lastMeasurement = now;
            number = 0;
            return {};
        }

        auto timeDifference = now - lastMeasurement;
        float averageResult = normalize(number, timeDifference);
        number = 0;
        lastMeasurement = now;
        return {tools::floatToString(averageResult, 4)};
    }

    void update() {
        ++number;
    }
private:
    EspApi& esp;
    float multiplier;
    unsigned number = 0;
    unsigned long lastMeasurement = 0;

    float normalize(float number, float timeDifference) {
        // mesure in ticks per second
        return number * 1000.0f / timeDifference * multiplier;
    }
};

CounterInterface::CounterInterface(std::ostream& debug, EspApi& esp,
        std::string name, uint8_t pin, int bounceTime, float multiplier,
        int interval, int offset, std::vector<std::string> pulse)
        : interval(interval),
          sensorInterface(debug, esp, createCounterSensor(esp, multiplier),
                  std::move(name), interval, offset, std::move(pulse)) {
    bounce.attach(pin, INPUT);
    bounce.interval(bounceTime);
}

auto CounterInterface::createCounterSensor(EspApi& esp, float multiplier)
        -> std::unique_ptr<CounterSensor> {
    auto result = std::make_unique<CounterSensor>(esp, multiplier);
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
    if (bounce.update() && bounce.rose()) {
        counterSensor->update();
    }
    sensorInterface.update(action);
}
