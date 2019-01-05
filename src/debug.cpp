#include "debug.hpp"
#include "client.hpp"

std::unique_ptr<WiFiDebugger> wifiDebugger;
std::unique_ptr<MqttDebugger> mqttDebugger;

WiFiClient& WiFiDebugger::getClient() {
    if (client && client.connected()) {
        return client;
    }
    client = server.available();
    return client;
}

void MqttDebugger::send() {
    if (sending) {
        return;
    }

    if (mqtt::isConnected()) {
        sending = true;
        mqtt::publish(topic, currentLine.get(), false);
        sending = false;
    }
    currentLine.clear();
}
