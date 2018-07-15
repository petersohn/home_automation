#ifndef GPIOINPUT_HPP
#define GPIOINPUT_HPP

#include "common/Interface.hpp"

class GpioInput : public Interface {
public:
    enum class CycleType {
        none, single, multi
    };

    GpioInput(int pin, CycleType cycleType, unsigned interval = 10);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    void onChange();

    const int pin;
    const CycleType cycleType;
    const unsigned interval;
    bool startup = false;
    volatile int state = 0;
    volatile unsigned long lastChanged = 0;
    volatile int cycles = 0;
};


#endif // GPIOINPUT_HPP
