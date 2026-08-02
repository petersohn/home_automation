#ifndef INTERFACE_CONFIGS_HPP
#define INTERFACE_CONFIGS_HPP

#include <string>
#include <variant>

#include "AnalogConfig.hpp"
#include "CounterConfig.hpp"
#include "CoverConfig.hpp"
#include "DallasTemperatureConfig.hpp"
#include "DhtConfig.hpp"
#include "EchoDistanceConfig.hpp"
#include "EncoderConfig.hpp"
#include "Hm3301Config.hpp"
#include "InputConfig.hpp"
#include "KeepaliveConfig.hpp"
#include "MqttInterfaceConfig.hpp"
#include "OutputConfig.hpp"
#include "PowerSupplyConfig.hpp"
#include "PwmConfig.hpp"
#include "StatusConfig.hpp"

using InterfaceConfigVariant = std::variant<
    InputConfig, OutputConfig, PwmConfig, AnalogConfig, EncoderConfig,
    DhtConfig, DallasTemperatureConfig, Hm3301Config, CounterConfig,
    EchoDistanceConfig, EchoDistanceReaderConfig, MqttInterfaceConfig,
    KeepaliveConfig, PowerSupplyConfig, CoverConfig, StatusConfig>;

struct InterfaceEntry {
    std::string name;
    std::string commandTopic;
    InterfaceConfigVariant config;
};

#endif  // INTERFACE_CONFIGS_HPP