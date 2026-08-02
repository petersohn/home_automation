#ifndef ACTION_CONFIG_HPP
#define ACTION_CONFIG_HPP

#include <string>
#include <variant>

struct PublishActionConfig {
    std::string topic;
    std::string operationJson;
    bool retain = false;
    unsigned minimumSendInterval = 0;
    double sendDiff = 0.0;
};

struct CommandActionConfig {
    std::string target;
    std::string operationJson;
};

using ActionConfigVariant =
    std::variant<PublishActionConfig, CommandActionConfig>;

struct ActionEntry {
    std::string interface;
    ActionConfigVariant config;
};

#endif  // ACTION_CONFIG_HPP