#ifndef ESPAPIIMPL_HPP
#define ESPAPIIMPL_HPP

#include "common/EspApi.hpp"

class EspApiImpl : public EspApi {
public:
    EspApiImpl() = default;

    virtual void pinMode(uint8_t pin, GpioMode mode) override;
    virtual void digitalWrite(uint8_t pin, uint8_t val) override;
    virtual int digitalRead(uint8_t pin) override;

    virtual unsigned long millis() override;
    virtual void delay(unsigned long ms) override;
    virtual void restart(bool hard) override;

    virtual uint32_t getFreeHeap() override;

    virtual void attachInterrupt(
        uint8_t pin, std::function<void(void)> intRoutine,
        InterruptMode mode) override;
    virtual void doDisableInterrupt() override;
    virtual void doEnableInterrupt() override;
};

#endif  // ESPAPIIMPL_HPP
