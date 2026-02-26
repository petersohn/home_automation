#ifndef OPERATIONPARSER2_HPP
#define OPERATIONPARSER2_HPP

#include <memory>
#include <ostream>
#include <unordered_set>
#include <vector>

#include "Operation.hpp"

class InterfaceConfig;

namespace operation {

class Parser2 {
public:
    Parser2(
        std::ostream& debug,
        const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
        InterfaceConfig* defaultInterface);

    std::unique_ptr<Operation> parse(const std::string& data);

    std::unordered_set<InterfaceConfig*>&& getUsedInterfaces() && {
        return std::move(usedInterfaces);
    }

private:
    std::ostream& debug;
    std::vector<InterfaceConfig*> interfaces;
    InterfaceConfig* defaultInterface;
    std::unordered_set<InterfaceConfig*> usedInterfaces;
};

}  // namespace operation

#endif  // OPERATIONPARSER2_HPP
