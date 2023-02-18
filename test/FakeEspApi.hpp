#ifndef TEST_FAKEESPAPI_HPP
#define TEST_FAKEESPAPI_HPP

#include "common/EspApi.hpp"

class FakeEspApi : public EspApi {
public:
    virtual void pinMode(uint8_t pin, GpioMode mode) override;
    virtual void digitalWrite(uint8_t pin, uint8_t val) override;
    virtual int digitalRead(uint8_t pin) override;
    virtual int analogRead(uint8_t pin) override;

    virtual unsigned long millis() override;
    virtual void delay(unsigned long ms) override;
    virtual void restart(bool hard) override;

    virtual uint32_t getFreeHeap() override;

    virtual void attachInterrupt(uint8_t pin,
        std::function<void(void)> intRoutine, InterruptMode mode) override;
    virtual void doDisableInterrupt() override;
    virtual void doEnableInterrupt() override;

    bool restarted = false;
private:
    unsigned long time = 0;
};


#endif // TEST_FAKEESPAPI_HPP
