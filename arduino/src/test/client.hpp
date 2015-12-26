#ifndef TEST_CLIENT_HPP
#define TEST_CLIENT_HPP

#include <Arduino.h>

template<typename Client>
bool connect(Client& client, const char* address, int port) {
    if (client.connected()) {
        return true;
    }
    Serial.print("Connecting to ");
    Serial.print(address);
    Serial.print(":");
    Serial.print(port);
    Serial.println("...");
    if (!client.connect(address, port)) {
        Serial.println("Connection failed.");
        return false;
    }
}


#endif // TEST_CLIENT_HPP
