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
    const auto now = this->esp.millis();
    if (now > static_cast<unsigned long>(this->offset)) {
        this->nextExecution = now - this->offset;
    } else {
        this->nextExecution = 1;
    }
    this->needToReset = true;
}

void SensorInterface::execute(const std::string& /*command*/) {}

void SensorInterface::update(Actions action) {
    auto now = this->esp.millis();
    if ((this->nextExecution != 0 && now >= this->nextExecution) ||
        (this->nextRetry != 0 && now >= this->nextRetry)) {
        auto values = this->sensor->measure();
        if (!values) {
            return;
        }
        if (now >= this->nextExecution) {
            this->nextExecution +=
                ((now - this->nextExecution) / this->interval + 1) *
                this->interval;
        }
        this->debug << this->name;
        if (values->empty()) {
            this->debug << ": Measurement failed. Trying again." << std::endl;
            this->nextRetry = now + 1000;
        } else {
            this->debug << ": Measurement successful:";
            for (const std::string& value : *values) {
                this->debug << " " << value;
            }
            this->debug << std::endl;
            this->nextRetry = 0;

            if (this->needToReset) {
                action.reset();
                this->needToReset = false;
            }

            action.fire(*values);
            this->pulseSent = false;
        }
    } else if (!this->pulse.empty()) {
        if (!this->pulseSent) {
            action.fire(this->pulse);
            this->pulseSent = true;
        }
    }
}
