#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <limits>
#include <ostream>
#include <string>
#include <vector>

#include "common/EspApi.hpp"
#include "common/InterfaceConfig.hpp"
#include "common/MqttClient.hpp"
#include "common/rtc.hpp"

class DebugStreambuf;

struct GlobalConfig {
    std::string wifiSSID;
    std::string wifiPassword;
    std::vector<ServerConfig> servers;
};

struct DeviceConfig {
    std::string name;
    TopicConfig topics;
    std::unique_ptr<std::streambuf> debug;
    int debugPort = 2534;
    std::string debugTopic;
    uint8_t resetPin = std::numeric_limits<uint8_t>::max();
    std::vector<std::unique_ptr<InterfaceConfig>> interfaces;

    DeviceConfig() = default;
    DeviceConfig(const DeviceConfig&) = delete;
    DeviceConfig& operator=(const DeviceConfig&) = delete;
    DeviceConfig(DeviceConfig&&) = default;
    DeviceConfig& operator=(DeviceConfig&&) = default;
};

extern GlobalConfig globalConfig;
extern DeviceConfig deviceConfig;

void initConfig(
    std::ostream& debug, DebugStreambuf& debugStream, EspApi& esp, Rtc& rtc,
    MqttClient& mqttClient);

#endif  // CONFIG_HPP
