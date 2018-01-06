#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "common/InterfaceConfig.hpp"

#include <string>
#include <vector>

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
