#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <Arduino.h>

#include <memory>
#include <vector>

class Interface;
class Action;

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
    std::vector<String> storedValue;
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

InterfaceConfig* findInterface(const String& name);
void initConfig();

#endif // CONFIG_HPP
