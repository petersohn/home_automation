#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "Action.hpp"
#include "Interface.hpp"

#include <Arduino.h>

#include <memory>
#include <vector>

struct GlobalConfig {
    String wifiSSID;
    String wifiPassword;
    String serverAddress;
    uint16_t serverPort = 0;
    String serverUsername;
    String serverPassword;
};

struct InterfaceConfig {
    String name;
    std::unique_ptr<Interface> interface;
    String commandTopic;
    std::vector<std::unique_ptr<Action>> actions;
};

struct DeviceConfig {
    String name;
    String availabilityTopic;
    bool debug = false;
    std::vector<InterfaceConfig> interfaces;
};

extern GlobalConfig globalConfig;
extern DeviceConfig deviceConfig;

void initConfig();

#endif // CONFIG_HPP
