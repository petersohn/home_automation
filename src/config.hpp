#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "common/InterfaceConfig.hpp"
#include "common/EspApi.hpp"
#include "common/rtc.hpp"

#include <string>
#include <vector>
#include <ostream>

class MqttClient;
class DebugStreambuf;

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
    std::unique_ptr<std::streambuf> debug;
    int debugPort = 2534;
    std::string debugTopic;
    std::vector<std::unique_ptr<InterfaceConfig>> interfaces;

    DeviceConfig() = default;
    DeviceConfig(const DeviceConfig&) = delete;
    DeviceConfig& operator=(const DeviceConfig&) = delete;
    DeviceConfig(DeviceConfig&&) = default;
	DeviceConfig& operator=(DeviceConfig&&) = default;
};

extern GlobalConfig globalConfig;
extern DeviceConfig deviceConfig;

void initConfig(std::ostream& debug, DebugStreambuf& debugStream, EspApi& esp,
    Rtc& rtc, MqttClient& mqttClient);

#endif // CONFIG_HPP
