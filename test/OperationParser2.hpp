#ifndef OPERATIONPARSER2_HPP
#define OPERATIONPARSER2_HPP

#include <memory>
#include <vector>

struct InterfaceConfig;

class OperationParser2 {
    OperationParser2(
        std::ostream& debug,
        const std::vector<std::unique_ptr<InterfaceConfig>>& interfaces,
        InterfaceConfig* defaultInterface);

public:
}

#endif  // OPERATIONPARSER2_HPP
