#ifndef COMMON_ESPAPI_HPP
#define COMMON_ESPAPI_HPP

#include <cstdint>
#include <functional>
#include <memory>

enum class InterruptMode {
    rise, fall, change
};

enum class GpioMode {
    input, inputPullup, output
};

class EspApi {
public:
    class InterruptGuard {
    public:
        InterruptGuard(EspApi& esp) : esp(esp) {
            esp.doDisableInterrupt();
        }
        ~InterruptGuard() {
            esp.doEnableInterrupt();
        }

        InterruptGuard(const InterruptGuard&) = delete;
        InterruptGuard& operator=(const InterruptGuard&) = delete;

    private:
        EspApi& esp;
    };

    EspApi() = default;
    EspApi(const EspApi&) = delete;
    EspApi& operator=(const EspApi&) = delete;

    virtual void pinMode(uint8_t pin, GpioMode mode) = 0;
    virtual void digitalWrite(uint8_t pin, uint8_t val) = 0;
    virtual int digitalRead(uint8_t pin) = 0;
    virtual int analogRead(uint8_t pin) = 0;

    virtual unsigned long millis() = 0;
    virtual void delay(unsigned long ms) = 0;
    virtual void restart() = 0;

    virtual uint32_t getFreeHeap() = 0;

    virtual void attachInterrupt(uint8_t pin,
        std::function<void(void)> intRoutine, InterruptMode mode) = 0;
    virtual void doDisableInterrupt() = 0;
    virtual void doEnableInterrupt() = 0;

    std::unique_ptr<InterruptGuard> disableInterrupt() {
        return std::make_unique<InterruptGuard>(*this);
    }
};

#endif // COMMON_ESPAPI_HPP
