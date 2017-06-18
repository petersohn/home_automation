#ifndef DEBUG_HPP
#define DEBUG_HPP

#include "config.hpp"

#include <Arduino.h>

#define DEBUG(...) if (::deviceConfig.debug) ::Serial.print(__VA_ARGS__)
#define DEBUGLN(...) if (::deviceConfig.debug) ::Serial.println(__VA_ARGS__)

#endif // DEBUG_HPP
