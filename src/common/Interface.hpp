#ifndef COMMON_INTERFACE_HPP
#define COMMON_INTERFACE_HPP

#include "Actions.hpp"

#include <string>

class Interface {
public:
    virtual void start() = 0;
    virtual void execute(const std::string& command) = 0;
    virtual void update(Actions action) = 0;
    virtual ~Interface() {}
};

#endif // COMMON_INTERFACE_HPP
