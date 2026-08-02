#ifndef GLOBAL_CONFIG_HPP
#define GLOBAL_CONFIG_HPP

#include <cstdint>
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

struct TopicConfig {
    std::string availabilityTopic;
    std::string statusTopic;
};

struct DeviceConfigCommon {
    std::string name;
    TopicConfig topics;
    int debugPort = 2534;
    std::string debugTopic;
    uint8_t resetPin = 255;
    bool debug = false;
};

#endif  // GLOBAL_CONFIG_HPP