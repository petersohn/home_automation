#include "WifiStream.hpp"

void WifiStreambuf::initClientIfNeeded() {
    if (client && client.connected()) {
        return;
    }
    client = server.available();
}

int WifiStreambuf::overflow(int ch) {
    initClientIfNeeded();
    if (client && client.connected()) {
        client.print(traits_type::to_char_type(ch));
    }
    return ch;
}

