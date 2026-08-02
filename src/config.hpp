#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <memory>
#include <streambuf>
#include <vector>

#include "common/GlobalConfig.hpp"
#include "common/InterfaceConfig.hpp"

class DebugStreambuf;
class EspApi;
class MqttClient;
class Rtc;

struct DeviceConfig {
    DeviceConfigCommon common;
    std::unique_ptr<std::streambuf> debug;
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