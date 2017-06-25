#include "GpioInterface.hpp"

#include "debug.hpp"

#include <ArduinoJson.h>

namespace {

template<typename T>
String createSimpleJson(const char* key, const T& value) {
    StaticJsonBuffer<64> buffer;
    JsonObject& root = buffer.createObject();
    root[key] = value;
    String result;
    root.printTo(result);
    return result;
}

bool getBoolValue(JsonVariant json, bool& output) {
    if (json.is<const char*>()) {
        String s = json.as<String>();
        if (s.equalsIgnoreCase("on") || s.equalsIgnoreCase("true")) {
            output = true;
            return true;
        }
        if (s.equalsIgnoreCase("off") || s.equalsIgnoreCase("false")) {
            output = false;
            return true;
        }
        return false;
    }
    if (json.is<bool>() || json.is<int>()) {
        output = json.as<bool>();
        return true;
    }
    return false;
}

} // unnamed namespace

void GpioInput::execute(const String& /*command*/) {
}

void GpioInput::update(Actions action) {
    if (bounce.update() || startup) {
        action.fire(createSimpleJson("value", bounce.read()));
        startup = false;
    }
}

void GpioOutput::execute(const String& command) {
    StaticJsonBuffer<64> buffer;
    JsonObject& root = buffer.parseObject(command);
    bool newValue = false;
    DEBUG("Pin: ");
    DEBUG(pin);
    DEBUGLN(": executing command: " + command);
    if (!getBoolValue(root["value"], newValue)) {
        DEBUGLN("Invalid command.");
        return;
    }
    DEBUG("Value: ");
    DEBUGLN(newValue);
    if (value != newValue) {
        digitalWrite(pin, newValue);
        value = newValue;
        changed = true;
    }
}

void GpioOutput::update(Actions action) {
    if (changed) {
        action.fire(createSimpleJson<bool>("value", digitalRead(pin)));
        changed = false;
    }
}
