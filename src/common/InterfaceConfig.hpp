#ifndef INTERFACECONFIG_HPP
#define INTERFACECONFIG_HPP

#include <memory>
#include <string>
#include <vector>

class Interface;
class Action;

struct InterfaceConfig {
    std::string name;
    std::unique_ptr<Interface> interface;
    std::vector<std::unique_ptr<Action>> actions;
    std::vector<std::string> storedValue;
};

#endif // INTERFACECONFIG_HPP
