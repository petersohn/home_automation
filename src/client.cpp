#include "client.hpp"

#include "ArduinoJson.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "common/Interface.hpp"
#include "tools/string.hpp"

#include <algorithm>
#include <vector>

static_assert(MQTT_MAX_PACKET_SIZE == 256, "check MQTT packet size");

namespace {

enum class ConnectStatus {
    connecting,
    connectionSuccessful,
    connectionFailed
};

PubSubClient client;

constexpr unsigned initialBackoff = 500;
constexpr unsigned maxBackoff = 60000;
constexpr unsigned statusSendInterval = 60000;
constexpr unsigned availabilityReceiveTimeout = 2000;

unsigned long nextConnectionAttempt = 0;
unsigned long availabilityReceiveTimeLimit = 0;
unsigned backoff = initialBackoff;
unsigned long nextStatusSend = 0;
bool initialized = false;
WiFiClient wifiClient;
std::string willMessage;
bool restarted = true;

using Subscription = std::pair<std::string,
      std::function<void(const std::string&)>>;

std::vector<Subscription> subscriptions;

std::string getMac() {
    std::uint8_t mac[6];
    WiFi.macAddress(mac);
    tools::Join result{":"};
    for (int i = 0; i < 6; ++i) {
        result.add(tools::intToString(mac[i], 16));
    }
    return result.get();
}

std::string getStatusMessage(bool available, bool restarted) {
    DynamicJsonBuffer buffer(200);
    JsonObject& message = buffer.createObject();
    message["name"] = deviceConfig.name.c_str();
    message["available"] = available;
    message["mac"] = getMac();
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

void resetAvailabilityReceive() {
    availabilityReceiveTimeLimit = 0;
}

void connectionBackoff() {
    nextConnectionAttempt = millis() + backoff;
    backoff = std::min(backoff * 2, maxBackoff);
}

void resetConnectionBackoff() {
    backoff = initialBackoff;
}

void handleAvailabilityMessage(const JsonObject& message) {
    bool available = message.get<bool>("available");
    auto name = message.get<std::string>("name");
    auto mac = message.get<std::string>("mac");

    debugln("Got availability: available=" + tools::intToString(available) +
            " name=" + name + " mac=" + mac);

    if (name != deviceConfig.name) {
        debugln("Another device, not interested.");
        return;
    }

    if (mac != getMac() && available) {
        debugln("Device collision.");
        client.disconnect();
        connectionBackoff();
    }

    if (availabilityReceiveTimeLimit != 0) {
        debugln("Got expected message.");
        resetAvailabilityReceive();
        nextStatusSend = millis();
    } else if (!available) {
        debugln("Refreshing availability state.");
        nextStatusSend = millis();
    }
}

void onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length) {
    std::string topicStr = topic;
    debugln("Message received on topic " + topicStr);
    if (topicStr == deviceConfig.availabilityTopic) {
        std::string payloadStr{reinterpret_cast<const char*>(payload), length};
        DynamicJsonBuffer buffer(200);
        auto& message = buffer.parseObject(payloadStr);
        handleAvailabilityMessage(message);
    }

    auto iterator = std::find_if(
            subscriptions.begin(), subscriptions.end(),
            [&topicStr](const Subscription& element) {
                return element.first == topicStr;
            });
    if (iterator == subscriptions.end()) {
        debugln("No subscription for topic.");
        return;
    }
    std::string message((const char*)payload, length);
    iterator->second(message);
}

bool tryToConnect(const ServerConfig& server) {
    debug("Connecting to ");
    debug(server.address);
    debug(":");
    debug(server.port);
    debug(" as ");
    debugln(server.username);
    bool result = false;
    client.setServer(server.address.c_str(), server.port).setClient(wifiClient)
            .setCallback(onMessageReceived);
    std::string clientId = deviceConfig.name + "-" + getMac();
    debugln("clientId=" + clientId);
    if (deviceConfig.availabilityTopic.length() != 0) {
        willMessage = getStatusMessage(false, false);
        result = client.connect(
                clientId.c_str(),
                server.username.c_str(),
                server.password.c_str(),
                deviceConfig.availabilityTopic.c_str(), 0, true,
                willMessage.c_str());
        if (result) {
            debugln("Connected to server. Listening to availability topic.");
            result = client.subscribe(deviceConfig.availabilityTopic.c_str());
            if (!result) {
                debugln("Failed to listen to availability topic.");
                client.disconnect();
            }
            availabilityReceiveTimeLimit =
                    millis() + availabilityReceiveTimeout;
        }
    } else {
        result = client.connect(
                clientId.c_str(),
                server.username.c_str(),
                server.password.c_str());
    }
    return result;
}

ConnectStatus connectIfNeeded() {
    if (!client.connected()) {
        initialized = false;
        debugln("Client status = " + tools::intToString(client.state()));
        debugln("Connecting to MQTT broker...");
        for (const ServerConfig& server : globalConfig.servers) {
            if (tryToConnect(server)) {
                debugln("Connection successful.");
                for (const auto& element : subscriptions) {
                    client.subscribe(element.first.c_str());
                }
                break;
            }
        }

        if (!client.connected()) {
            debugln("Connection failed.");
            return ConnectStatus::connectionFailed;
        }
    }

    if (availabilityReceiveTimeLimit != 0) {
        if (availabilityReceiveTimeLimit > millis()) {
            return ConnectStatus::connecting;
        }

        debugln("Did not get availability topic in time. "
                "Assuming we are first.");
        resetAvailabilityReceive();
    }
    return ConnectStatus::connectionSuccessful;
}

void sendStatusMessage(bool restarted) {
    auto now = millis();
    if (deviceConfig.availabilityTopic.length() != 0 && now >= nextStatusSend) {
        debugln("Sending status message to topic "
                + deviceConfig.availabilityTopic);
        std::string message = getStatusMessage(true, restarted);
        debugln(message);
        if (client.publish(deviceConfig.availabilityTopic.c_str(),
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

void loop() {
    if (millis() >= nextConnectionAttempt) {
        switch (connectIfNeeded()) {
        case ConnectStatus::connectionSuccessful:
            if (initialized) {
                sendStatusMessage(false);
            } else {
                for (const auto& interface : deviceConfig.interfaces) {
                    interface->interface->start();
                }
                nextStatusSend = millis();
                sendStatusMessage(restarted);
                restarted = false;
                resetConnectionBackoff();
                initialized = true;
            }
            break;
        case ConnectStatus::connectionFailed:
            connectionBackoff();
            break;
        case ConnectStatus::connecting:
            break;
        }
    }

    client.loop();
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

void publish(
        const std::string& topic, const std::string& payload, bool retain) {
    debugln("Publishing to " + topic);
    if (client.publish(topic.c_str(), payload.c_str(), retain)) {
        debugln("Success.");
    } else {
        debugln("Failure.");
    }
}

void disconnect() {
    mqtt::disconnect();
}

} // namespace mqtt
