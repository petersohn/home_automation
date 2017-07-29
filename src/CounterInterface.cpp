#include "CounterInterface.hpp"

class CounterInterface::CounterSensor : public Sensor {
public:
    CounterSensor(float multiplier) : multiplier(multiplier) {}

    std::vector<String> measure() override {
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
        return {String{result}};
    }

    void increment() { ++number; }
private:
    float multiplier;
    unsigned number = 0;
    unsigned long lastMeasurement = 0;
};

CounterInterface::CounterInterface(
        int pin, float multiplier, int interval, int offset)
        : sensorInterface(createCounterSensor(multiplier), interval, offset) {
    pinMode(pin, INPUT);
    bounce.attach(pin);
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

void CounterInterface::execute(const String& command) {
    sensorInterface.execute(command);
}

void CounterInterface::update(Actions action) {
    if (bounce.update() && bounce.rose()) {
        counterSensor->increment();
    }
    sensorInterface.update(action);
}
