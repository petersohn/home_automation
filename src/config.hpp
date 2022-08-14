#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "common/InterfaceConfig.hpp"

#include <string>
#include <vector>
#include <ostream>

class MqttClient;

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
    std::string debugTopic;
    std::vector<std::unique_ptr<InterfaceConfig>> interfaces;
};

extern GlobalConfig globalConfig;
extern DeviceConfig deviceConfig;

void initConfig(std::ostream& debug, MqttClient& mqttClient);

#endif // CONFIG_HPP
