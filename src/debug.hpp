#ifndef DEBUG_HPP
#define DEBUG_HPP

#include "config.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>

#define DEBUG(...) if (::deviceConfig.debug) ::Serial.print(__VA_ARGS__)
#define DEBUGLN(...) if (::deviceConfig.debug) ::Serial.println(__VA_ARGS__)

inline void debugJson(JsonVariant value) {
    if (!deviceConfig.debug) {
        return;
    }

    StaticJsonBuffer<50> buffer;
    JsonArray& array = buffer.createArray();
    array.add(value);
    array.prettyPrintTo(Serial);
}

#endif // DEBUG_HPP
