#ifndef POWERSUPPLYINTERFACE_HPP
#define POWERSUPPLYINTERFACE_HPP

#include "common/Interface.hpp"

class PowerSupplyInterface : public Interface {
public:
    enum class TargetState {
        Off, On, Dontcare
    };

    PowerSupplyInterface(uint8_t powerSwitchPin, uint8_t resetSwitchPin,
            uint8_t powerCheckPin, unsigned pushTime, unsigned forceOffTime,
            unsigned checkTime, const std::string& initialState);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    const uint8_t powerSwitchPin;
    const uint8_t resetSwitchPin;
    const uint8_t powerCheckPin;
    const unsigned pushTime;
    const unsigned forceOffTime;
    const unsigned checkTime;
    TargetState targetState;
    unsigned nextCheck = 0;
    unsigned powerButtonRelease = 0;
    unsigned resetButtonRelease = 0;

    void pullDown(uint8_t pin);
    void release(uint8_t pin);
};

#endif // POWERSUPPLYINTERFACE_HPP
