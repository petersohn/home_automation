#include "WifiStream.hpp"

#include <Arduino.h>

void WifiStreambuf::initClientIfNeeded() {
    if (client && client.connected()) {
        return;
    }
    const auto now = millis();
    if (lastChecked + 100 > now) {
        return;
    }
    lastChecked = now;
    client = server.available();
}

int WifiStreambuf::overflow(int ch) {
    initClientIfNeeded();
    if (client && client.connected()) {
        client.print(traits_type::to_char_type(ch));
    }
    return ch;
}

