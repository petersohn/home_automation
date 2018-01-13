#include "client.hpp"

#include "ArduinoJson.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "common/Interface.hpp"
#include "tools/string.hpp"

#include <algorithm>
#include <vector>

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
std::string willMessage;
bool restarted = true;

using Subscription = std::pair<std::string,
      std::function<void(const std::string&)>>;

std::vector<Subscription> subscriptions;

void onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length) {
    std::string topicStr = topic;
    debugln("Message received on topic " + topicStr);
    auto iterator = std::find_if(
            subscriptions.begin(), subscriptions.end(),
            [&topicStr](const Subscription& element) {
                debugln("-->" + element.first);
                return element.first == topicStr;
            });
    if (iterator == subscriptions.end()) {
        debugln("No subscription for topic.");
        return;
    }
    std::string message((const char*)payload, length);
    iterator->second(message);
}

std::string getStatusMessage(bool available, bool restarted) {
    StaticJsonBuffer<128> buffer;
    JsonObject& message = buffer.createObject();
    message["name"] = deviceConfig.name.c_str();
    message["available"] = available;
    if (available) {
        message["restarted"] = restarted;
        message["ip"] = WiFi.localIP().toString();
        message["uptime"] = millis();
        message["rssi"] = WiFi.RSSI();
        message["freeMemory"] = ESP.getFreeHeap();
    }
    std::string result;
    message.printTo(result);
    return result;
}

bool tryToConnect(const ServerConfig& server) {
    debug("Connecting to ");
    debug(server.address);
    debug(":");
    debug(server.port);
    debug(" as ");
    debugln(server.username);
    bool result = false;
    if (deviceConfig.availabilityTopic.length() != 0) {
        willMessage = getStatusMessage(false, false);
        mqtt::client.setServer(server.address.c_str(), server.port)
                .setCallback(onMessageReceived).setClient(wifiClient);
        result = mqtt::client.connect(
                deviceConfig.name.c_str(),
                server.username.c_str(),
                server.password.c_str(),
                deviceConfig.availabilityTopic.c_str(), 0, true,
                willMessage.c_str());
    } else {
        result = mqtt::client.connect(
                deviceConfig.name.c_str(),
                server.username.c_str(),
                server.password.c_str());
    }
    return result;
}

ConnectStatus connectIfNeeded() {
    if (mqtt::client.connected()) {
        return ConnectStatus::alreadyConnected;
    }
    debugln("Client status = " + std::to_string(mqtt::client.state()));
    debugln("Connecting to MQTT broker...");
    for (const ServerConfig& server : globalConfig.servers) {
        if (tryToConnect(server)) {
            debugln("Connection successful.");
            for (const auto& element : subscriptions) {
                mqtt::client.subscribe(element.first.c_str());
            }
            return ConnectStatus::connectionSuccessful;
        }
    }
    debugln("Connection failed.");
    return ConnectStatus::connectionFailed;
}

void sendStatusMessage(bool restarted) {
    auto now = millis();
    if (deviceConfig.availabilityTopic.length() != 0 && now >= nextStatusSend) {
        debugln("Sending status message to topic "
                + deviceConfig.availabilityTopic);
        std::string message = getStatusMessage(true, restarted);
        debugln(message);
        if (mqtt::client.publish(deviceConfig.availabilityTopic.c_str(),
                message.c_str(), true)) {
            debugln("Success.");
        } else {
            debugln("Failure.");
        }
        nextStatusSend += ((now - nextStatusSend) / statusSendInterval + 1)
                * statusSendInterval;
    }
}

} // unnamed namespace

namespace mqtt {

PubSubClient client;

void loop() {
    if (millis() >= nextConnectionAttempt) {
        switch (connectIfNeeded()) {
        case ConnectStatus::alreadyConnected:
            sendStatusMessage(false);
            break;
        case ConnectStatus::connectionSuccessful:
            for (const auto& interface : deviceConfig.interfaces) {
                interface->interface->start();
            }
            nextStatusSend = millis();
            sendStatusMessage(restarted);
            restarted = false;
            break;
        case ConnectStatus::connectionFailed:
            nextConnectionAttempt = millis() + connectionAttemptInterval;
            break;
        }
    }
}

void subscribe(const std::string& topic,
        std::function<void(const std::string&)> callback) {
    subscriptions.emplace_back(topic, callback);
    if (client.connected()) {
        client.subscribe(topic.c_str());
    }
}

void unsubscribe(const std::string& topic) {
    subscriptions.erase(std::remove_if(
            subscriptions.begin(), subscriptions.end(),
            [&topic](const Subscription& element) {
                return element.first == topic;
            }), subscriptions.end());
    if (client.connected()) {
        client.unsubscribe(topic.c_str());
    }
}

} // namespace mqtt
