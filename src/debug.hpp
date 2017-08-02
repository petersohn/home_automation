#ifndef debug_HPP
#define debug_HPP

#include "config.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>

template<typename T>
void debug(const T& value) {
    if (deviceConfig.debug) {
        Serial.print(value);
    }
}

template<typename T = char*>
void debugln(const T& value = "") {
    if (deviceConfig.debug) {
        Serial.println(value);
    }
}

inline void debugJson(JsonVariant value) {
    if (!deviceConfig.debug) {
        return;
    }

    StaticJsonBuffer<50> buffer;
    JsonArray& array = buffer.createArray();
    array.add(value);
    array.prettyPrintTo(Serial);
}

#endif // debug_HPP
