#ifndef TEST_FAKEESPAPI_HPP
#define TEST_FAKEESPAPI_HPP

#include <map>
#include <optional>

#include "common/EspApi.hpp"

class FakeEspApi : public EspApi {
public:
    virtual void pinMode(uint8_t pin, GpioMode mode) override;
    virtual void digitalWrite(uint8_t pin, uint8_t val) override;
    virtual int digitalRead(uint8_t pin) override;

    virtual unsigned long millis() override;
    virtual unsigned long micros() override;
    virtual void delay(unsigned long ms) override;
    virtual void restart(bool hard) override;

    virtual uint32_t getFreeHeap() override;

    virtual void doDisableInterrupt() override;
    virtual void doEnableInterrupt() override;

    virtual void setRush(unsigned long microseconds) override;

    bool restarted = false;

    // Returns the mode a pin was last set to via pinMode(), or nullopt if the
    // pin was never configured.
    std::optional<GpioMode> getPinMode(uint8_t pin) const;

private:
    unsigned long time = 0;
    std::map<uint8_t, bool> pinValues;
    std::map<uint8_t, GpioMode> pinModes;
};

#endif  // TEST_FAKEESPAPI_HPP
