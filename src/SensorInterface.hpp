#ifndef SENSORINTERFACE_HPP
#define SENSORINTERFACE_HPP

#include "Interface.hpp"
#include "Sensor.hpp"

#include <memory>

class SensorInterface : public Interface {
public:
    SensorInterface(std::unique_ptr<Sensor>&& sensor,
            int interval, int offset)
            : sensor(std::move(sensor)), interval(interval),
            nextExecution(-offset) {}

    void execute(const String& command) override;
    void update(Actions action) override;

private:
    std::unique_ptr<Sensor> sensor;
    int interval;
    long nextExecution;
    long nextRetry = 0;
};

#endif // SENSORINTERFACE_HPP
