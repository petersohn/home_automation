#include "client.hpp"

#include "config.hpp"
#include "debug.hpp"
#include "string.hpp"

#include <algorithm>

namespace {

enum class ConnectStatus {
    alreadyConnected,
    connectionSuccessful,
    connectionFailed
};

constexpr unsigned connectionAttemptInterval = 1000;
constexpr unsigned statusSendInterval = 60000;

unsigned long nextConnectionAttempt = 0;
unsigned long nextStatusSend = 0;
WiFiClient wifiClient;
String willMessage;

void onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length) {
    String topicStr = topic;
    debugln("Message received on topic " + topicStr);
    auto interface = std::find_if(
            deviceConfig.interfaces.begin(), deviceConfig.interfaces.end(),
            [&topicStr](const InterfaceConfig& interface) {
                return interface.commandTopic == topicStr;
            });
    if (interface == deviceConfig.interfaces.end()) {
        debugln("Could not find appropriate interface.");
        return;
    }

    interface->interface->execute(tools::toString((char*)payload, length));
}

String getStatusMessage(bool available, std::int32_t rssi,
        const IPAddress ip, unsigned long uptime) {
    StaticJsonBuffer<128> buffer;
    JsonObject& message = buffer.createObject();
    message["name"] = deviceConfig.name.c_str();
    message["available"] = available;
    message["ip"] = ip.toString();
    message["uptime"] = uptime;
    message["rssi"] = rssi;
    String result;
    message.printTo(result);
    return result;
}

ConnectStatus connectIfNeeded() {
    if (mqtt::client.connected()) {
        return ConnectStatus::alreadyConnected;
    }
    debugln("Client status = " + String(mqtt::client.state()));
    debugln("Connecting to MQTT broker...");
    bool result = false;
    if (deviceConfig.availabilityTopic.length() != 0) {
        willMessage = getStatusMessage(false, 0, IPAddress{}, 0);
        debug("Connecting to ");
        debug(globalConfig.serverAddress);
        debug(":");
        debug(globalConfig.serverPort);
        debug(" as ");
        debugln(globalConfig.serverUsername);
        mqtt::client.setServer(globalConfig.serverAddress.c_str(),
                        globalConfig.serverPort)
                .setCallback(onMessageReceived).setClient(wifiClient);
        result = mqtt::client.connect(
                deviceConfig.name.c_str(),
                globalConfig.serverUsername.c_str(),
                globalConfig.serverPassword.c_str(),
                deviceConfig.availabilityTopic.c_str(), 0, true,
                willMessage.c_str());
    } else {
        result = mqtt::client.connect(
                deviceConfig.name.c_str(),
                globalConfig.serverUsername.c_str(),
                globalConfig.serverPassword.c_str());
    }
    if (result) {
        debugln("Connection successful.");
        for (const InterfaceConfig& interface : deviceConfig.interfaces) {
            if (interface.commandTopic.length() != 0) {
                mqtt::client.subscribe(interface.commandTopic.c_str());
            }
        }
        return ConnectStatus::connectionSuccessful;
    } else {
        debugln("Connection failed.");
        return ConnectStatus::connectionFailed;
    }
}

void sendStatusMessage() {
    auto now = millis();
    if (deviceConfig.availabilityTopic.length() != 0 && now >= nextStatusSend) {
        debug("Sending status message");
        String message = getStatusMessage(true, WiFi.RSSI(), WiFi.localIP(),
                millis());
        mqtt::client.publish(deviceConfig.availabilityTopic.c_str(),
                message.c_str(), true);
        nextStatusSend += ((now - nextStatusSend) / statusSendInterval + 1)
                * statusSendInterval;
    }
}

} // unnamed namespace

namespace mqtt {

PubSubClient client;

bool loop() {
    if (millis() >= nextConnectionAttempt) {
        switch (connectIfNeeded()) {
        case ConnectStatus::alreadyConnected:
            sendStatusMessage();
            break;
        case ConnectStatus::connectionSuccessful:
            for (const InterfaceConfig& interface :
                    deviceConfig.interfaces) {
                interface.interface->start();
            }
            nextStatusSend = millis();
            sendStatusMessage();
            break;
        case ConnectStatus::connectionFailed:
            nextConnectionAttempt = millis() + connectionAttemptInterval;
            break;
        }
    }
}

} // namespace mqtt
