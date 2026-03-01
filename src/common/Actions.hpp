#ifndef COMMON_ACTIONS_HPP
#define COMMON_ACTIONS_HPP

#include "Action.hpp"

class Actions {
public:
    Actions(InterfaceConfig& interface) : interface(interface) {}

    void fire(const std::vector<std::string>& values);
    void reset();

private:
    InterfaceConfig& interface;
};

#endif  // COMMON_ACTIONS_HPP
