#ifndef SENSORINTERFACE_HPP
#define SENSORINTERFACE_HPP

#include "common/Interface.hpp"
#include "common/Sensor.hpp"

#include <memory>

class SensorInterface : public Interface {
public:
    SensorInterface(std::unique_ptr<Sensor>&& sensor, std::string name,
            int interval, int offset, std::vector<std::string> pulse)
            : sensor(std::move(sensor)), name(std::move(name)),
              interval(interval), offset(offset), pulse(std::move(pulse)) {}

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    std::unique_ptr<Sensor> sensor;
    std::string name;
    int interval;
    int offset;
    long nextExecution = 0;
    long nextRetry = 0;
    std::vector<std::string> pulse;
    bool pulseSent = true;
};

#endif // SENSORINTERFACE_HPP
