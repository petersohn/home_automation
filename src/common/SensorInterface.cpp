#include "SensorInterface.hpp"

SensorInterface::SensorInterface(
    std::ostream& debug, EspApi& esp, std::unique_ptr<Sensor>&& sensor,
    std::string name, int interval, int offset, std::vector<std::string> pulse)
    : debug(debug)
    , esp(esp)
    , sensor(std::move(sensor))
    , name(std::move(name))
    , interval(interval)
    , offset(offset)
    , pulse(std::move(pulse)) {}

void SensorInterface::start() {
    // When connected to the network, all sensors make a measurement.
    // Afterwards, measurements are shifted by offset.
    nextExecution = esp.millis() - offset;
}

void SensorInterface::execute(const std::string& /*command*/) {}

void SensorInterface::update(Actions action) {
    auto now = esp.millis();
    if ((nextExecution != 0 && now >= nextExecution) ||
        (nextRetry != 0 && now >= nextRetry)) {
        auto values = sensor->measure();
        if (!values) {
            return;
        }
        if (now >= nextExecution) {
            nextExecution += ((now - nextExecution) / interval + 1) * interval;
        }
        debug << name;
        if (values->empty()) {
            debug << ": Measurement failed. Trying again." << std::endl;
            nextRetry = now + 1000;
        } else {
            debug << ": Measurement successful:";
            for (const std::string& value : *values) {
                debug << " " << value;
            }
            debug << std::endl;
            nextRetry = 0;
            action.fire(*values);
            pulseSent = false;
        }
    } else if (!pulse.empty()) {
        if (!pulseSent) {
            action.fire(pulse);
            pulseSent = true;
        }
    }
}
