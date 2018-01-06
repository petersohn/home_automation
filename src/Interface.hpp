#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include "Action.hpp"

#include <string>

class Interface {
public:
    virtual void start() = 0;
    virtual void execute(const std::string& command) = 0;
    virtual void update(Actions action) = 0;
    virtual ~Interface() {}
};

#endif // INTERFACE_HPP
