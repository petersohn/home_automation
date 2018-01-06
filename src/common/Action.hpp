#ifndef COMMON_ACTION_HPP
#define COMMON_ACTION_HPP

#include "InterfaceConfig.hpp"

#include <memory>
#include <string>
#include <vector>

class Action {
public:
    virtual void fire(const InterfaceConfig& interface) = 0;
    virtual ~Action() {}
};

#endif // COMMON_ACTION_HPP
