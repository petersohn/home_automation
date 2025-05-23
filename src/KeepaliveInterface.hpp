#ifndef KEEPALIVEINTERFACE_HPP
#define KEEPALIVEINTERFACE_HPP

#include "common/EspApi.hpp"
#include "common/Interface.hpp"

class KeepaliveInterface : public Interface {
public:
    KeepaliveInterface(
        EspApi& esp, uint8_t pin, unsigned interval, unsigned resetInterval);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    EspApi& esp;

    void reset();

    const uint8_t pin;
    const unsigned interval;
    const unsigned resetInterval;
    unsigned nextReset = 0;
};

#endif  // KEEPALIVEINTERFACE_HPP
