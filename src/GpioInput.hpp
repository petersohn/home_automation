#ifndef GPIOINPUT_HPP
#define GPIOINPUT_HPP

#include "common/Interface.hpp"

#include <ostream>

class GpioInput : public Interface {
public:
    enum class CycleType {
        none, single, multi
    };

    GpioInput(std::ostream& debug, uint8_t pin, CycleType cycleType,
        unsigned interval = 10);

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


#endif // GPIOINPUT_HPP
