#include <iostream>

#include "common/ArduinoJson.hpp"
#include "common/Interface.hpp"
#include "common/InterfaceConfig.hpp"
#include "operation/OperationParser.hpp"

using namespace ArduinoJson;

void printUsage() {
    std::cerr << "Usage: operation_tester -i name value [value...] [-i name "
                 "value...]...\n"
                 "stdin: valid JSON containing action";
}

int main(int argc, const char** argv) {
    std::vector<std::string> args;
    args.reserve(argc - 1);

    std::vector<std::unique_ptr<InterfaceConfig>> interfaces;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "-i") {
            if (!interfaces.empty() &&
                (interfaces.back()->name.empty() ||
                 interfaces.back()->storedValue.empty())) {
                printUsage();
                return 1;
            }

            interfaces.emplace_back(std::make_unique<InterfaceConfig>());
        } else {
            auto& interface = *interfaces.back();
            if (interface.name.empty()) {
                interface.name = std::move(arg);
            } else {
                interface.storedValue.push_back(std::move(arg));
            }
        }
    }

    if (interfaces.empty()) {
        printUsage();
        return 1;
    }

    DynamicJsonBuffer buffer{512};
    std::string content, line;
    while (std::getline(std::cin, line)) {
        content += line;
        content += "\n";
    }
    auto& data = buffer.parseObject(content);
    if (!data.success()) {
        std::cerr << "Invalid JSON.\n";
        return 2;
    }

    const std::string interfaceName = data["interface"];
    InterfaceConfig* defaultInterface = nullptr;
    if (!interfaceName.empty()) {
        auto it = std::find_if(
            interfaces.begin(), interfaces.end(), [&](const auto& interface) {
            return interface->name == interfaceName;
        });
        if (it != interfaces.end()) {
            defaultInterface = it->get();
        }
    }
    operation::Parser parser{interfaces, defaultInterface};
    const std::string type = data["type"];
    const char* fieldName = type == "command" ? "command" : "payload";
    auto operation = parser.parse(data, fieldName, "template");
    std::cout << operation->evaluate() << "\n";

    return 0;
}
