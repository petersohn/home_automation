#ifndef TEST_CLIENT_HPP
#define TEST_CLIENT_HPP

#include "debug.hpp"

#include <Arduino.h>

template<typename Client>
bool connect(Client& client, const char* address, int port) {
    if (client.connected()) {
        return true;
    }
    DEBUG("Connecting to ");
    DEBUG(address);
    DEBUG(":");
    DEBUG(port);
    DEBUGLN("...");
    if (!client.connect(address, port)) {
        DEBUGLN("Connection failed.");
        return false;
    }
}


#endif // TEST_CLIENT_HPP
