#ifndef GPIOINPUT_HPP
#define GPIOINPUT_HPP

#include <ostream>

#include "common/InputConfig.hpp"
#include "common/Interface.hpp"

class GpioInput : public Interface {
public:
    GpioInput(std::ostream& debug, const InputConfig& config);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    std::ostream& debug;

    const uint8_t pin;
    const CycleType cycleType;
    const unsigned interval;
    bool startup = false;
    volatile int state = 0;
    volatile unsigned long lastChanged = 0;
    volatile int cycles = 0;

    static void onChangeStatic(void* arg);
    void onChange(/*bool newState*/);
};

#endif  // GPIOINPUT_HPP
