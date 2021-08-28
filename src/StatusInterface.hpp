#ifndef STATUSINTERFACE_HPP
#define STATUSINTERFACE_HPP

#include "common/Interface.hpp"

class StatusInterface : public Interface {
public:
    StatusInterface() {}

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    int value = -1;
};

#endif // STATUSINTERFACE_HPP

