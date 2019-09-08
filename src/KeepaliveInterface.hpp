#ifndef KEEPALIVEINTERFACE_HPP
#define KEEPALIVEINTERFACE_HPP

#include "common/Interface.hpp"

class KeepaliveInterface : public Interface {
public:
    KeepaliveInterface(int pin, unsigned interval);

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    void reset();

    const int pin;
    const unsigned interval;
    unsigned nextReset = 0;
};

#endif // KEEPALIVEINTERFACE_HPP

