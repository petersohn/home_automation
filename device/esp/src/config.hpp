#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "Interface.hpp"

#include <Arduino.h>

#include <memory>
#include <vector>

struct GlobalConfig {
	String wifiSSID;
	String wifiPassword;
	String serverAddress;
	uint16_t serverPort = 0;
	String serverPassword;
};

struct InterfaceConfig {
	String name;
	String lastValue;
	std::unique_ptr<Interface> interface;
};

struct DeviceConfig {
	String name;
	bool debug = true;
	std::vector<InterfaceConfig> interfaces;
};

extern GlobalConfig globalConfig;
extern DeviceConfig deviceConfig;

void initConfig();

std::vector<InterfaceConfig*> getModifiedInterfaces();

#endif // CONFIG_HPP
