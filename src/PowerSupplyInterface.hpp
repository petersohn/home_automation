#ifndef POWERSUPPLYINTERFACE_HPP
#define POWERSUPPLYINTERFACE_HPP

#include "common/Interface.hpp"

class PowerSupplyInterface : public Interface {
public:
    enum class TargetState {
        Off, On, Dontcare
    };

    PowerSupplyInterface(int powerSwitchPin, int resetSwitchPin,
            int powerCheckPin, unsigned pushTime, unsigned forceOffTime,
            unsigned checkTime, const std::string& initialState);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    const int powerSwitchPin;
    const int resetSwitchPin;
    const int powerCheckPin;
    const unsigned pushTime;
    const unsigned forceOffTime;
    const unsigned checkTime;
    TargetState targetState;
    unsigned nextCheck = 0;
    unsigned powerButtonRelease = 0;
    unsigned resetButtonRelease = 0;
};

#endif // POWERSUPPLYINTERFACE_HPP
