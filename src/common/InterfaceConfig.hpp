#ifndef INTERFACECONFIG_HPP
#define INTERFACECONFIG_HPP

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

class Interface;
class Action;

struct InterfaceConfig {
    std::string name;
    std::unique_ptr<Interface> interface;
    std::vector<std::shared_ptr<Action>> actions;
    std::vector<std::string> storedValue;
    bool hasExternalAction = false;
    bool hasInternalAction = false;

    InterfaceConfig() = default;
    InterfaceConfig(const InterfaceConfig&) = delete;
    InterfaceConfig& operator=(const InterfaceConfig&) = delete;
    InterfaceConfig(InterfaceConfig&&) = delete;
    InterfaceConfig& operator=(InterfaceConfig&&) = delete;
};

namespace detail {

class InterfaceHasName {
public:
    InterfaceHasName(const std::string& name) : name(name) {}

    template <typename InterfaceConfigPtr>
    bool operator()(const InterfaceConfigPtr& interface) {
        return interface->name == name;
    }

private:
    std::string name;
};

}  // namespace detail

template <typename Interfaces>
InterfaceConfig* findInterface(
    const Interfaces& interfaces, const std::string& name) {
    auto iterator = std::find_if(
        interfaces.begin(), interfaces.end(), detail::InterfaceHasName{name});
    if (iterator == interfaces.end()) {
        return nullptr;
    }
    return &**iterator;
}

#endif  // INTERFACECONFIG_HPP
