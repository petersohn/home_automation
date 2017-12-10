#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "Action.hpp"
#include "Interface.hpp"

#include <Arduino.h>

#include <memory>
#include <vector>

struct ServerConfig {
    String address;
    uint16_t port = 0;
    String username;
    String password;
};

struct GlobalConfig {
    String wifiSSID;
    String wifiPassword;
    std::vector<ServerConfig> servers;
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
    int debugPort = 2534;
    std::vector<InterfaceConfig> interfaces;
};

extern GlobalConfig globalConfig;
extern DeviceConfig deviceConfig;

void initConfig();

#endif // CONFIG_HPP
