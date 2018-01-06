#ifndef GPIOINPUT_HPP
#define GPIOINPUT_HPP

#include "common/Interface.hpp"

#include <Bounce2.h>

class GpioInput : public Interface {
public:
    GpioInput(int pin);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    bool startup = false;
    Bounce bounce;
};


#endif // GPIOINPUT_HPP
