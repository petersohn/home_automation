#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <Arduino.h>

#include <memory>
#include <vector>

class Interface;
class Action;

struct ServerConfig {
    std::string address;
    uint16_t port = 0;
    std::string username;
    std::string password;
};

struct GlobalConfig {
    std::string wifiSSID;
    std::string wifiPassword;
    std::vector<ServerConfig> servers;
};

struct InterfaceConfig {
    std::string name;
    std::unique_ptr<Interface> interface;
    std::vector<std::unique_ptr<Action>> actions;
    std::vector<std::string> storedValue;
};

struct DeviceConfig {
    std::string name;
    std::string availabilityTopic;
    bool debug = false;
    int debugPort = 2534;
    std::vector<InterfaceConfig> interfaces;
};

extern GlobalConfig globalConfig;
extern DeviceConfig deviceConfig;

InterfaceConfig* findInterface(const std::string& name);
void initConfig();

#endif // CONFIG_HPP
