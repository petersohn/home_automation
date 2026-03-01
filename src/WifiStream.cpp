#include "WifiStream.hpp"

#include <Arduino.h>

void WifiStreambuf::initClientIfNeeded() {
    if (this->client && this->client.connected()) {
        return;
    }
    const auto now = millis();
    if (this->lastChecked + 100 > now) {
        return;
    }
    this->lastChecked = now;
    this->client = this->server.available();
}

int WifiStreambuf::overflow(int ch) {
    initClientIfNeeded();
    if (this->client && this->client.connected()) {
        this->client.print(traits_type::to_char_type(ch));
    }
    return ch;
}
