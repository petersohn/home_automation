#ifndef KEEPALIVEINTERFACE_HPP
#define KEEPALIVEINTERFACE_HPP

#include "common/EspApi.hpp"
#include "common/Interface.hpp"
#include "common/KeepaliveConfig.hpp"

class KeepaliveInterface : public Interface {
public:
    KeepaliveInterface(EspApi& esp, const KeepaliveConfig& config);

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
