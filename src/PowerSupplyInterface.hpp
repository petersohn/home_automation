#ifndef POWERSUPPLYINTERFACE_HPP
#define POWERSUPPLYINTERFACE_HPP

#include <ostream>

#include "common/EspApi.hpp"
#include "common/Interface.hpp"
#include "common/PowerSupplyConfig.hpp"

class PowerSupplyInterface : public Interface {
public:
    enum class TargetState { Off, On, Dontcare };

    PowerSupplyInterface(
        std::ostream& debug, EspApi& esp, const PowerSupplyConfig& config);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    std::ostream& debug;
    EspApi& esp;

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

#endif  // POWERSUPPLYINTERFACE_HPP
