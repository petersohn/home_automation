#include "client.hpp"

#include "ArduinoJson.hpp"
#include "config.hpp"
#include "common/Interface.hpp"
#include "tools/string.hpp"
#include "ArduinoJson.hpp"

#include <ESP8266WiFi.h>

#include <algorithm>
#include <vector>

static_assert(MQTT_MAX_PACKET_SIZE == 256, "check MQTT packet size");

using namespace ArduinoJson;

namespace {

constexpr unsigned initialBackoff = 500;
constexpr unsigned maxBackoff = 60000;
constexpr unsigned statusSendInterval = 60000;
constexpr unsigned availabilityReceiveTimeout = 2000;

} // unnamed namespace

MqttClient::MqttClient(std::ostream& debug)
    : debug(debug)
    , backoff(initialBackoff)
    , wifiClient(std::make_unique<WiFiClient>())
    , client(std::make_unique<PubSubClient>())
{
}

std::string MqttClient::getMac() {
    std::uint8_t mac[6];
    WiFi.macAddress(mac);
    tools::Join result{":"};
    for (int i = 0; i < 6; ++i) {
        result.add(tools::intToString(mac[i], 16));
    }
    return result.get();
}

std::string MqttClient::getStatusMessage(bool available, bool restarted) {
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

void MqttClient::resetAvailabilityReceive() {
    availabilityReceiveTimeLimit = 0;
}

void MqttClient::connectionBackoff() {
    nextConnectionAttempt = millis() + backoff;
    backoff = std::min(backoff * 2, maxBackoff);
}

void MqttClient::resetConnectionBackoff() {
    backoff = initialBackoff;
}

void MqttClient::handleAvailabilityMessage(const JsonObject& message) {
    bool available = message.get<bool>("available");
    auto name = message.get<std::string>("name");
    auto mac = message.get<std::string>("mac");

    debug << "Got availability: available=" << tools::intToString(available)
        << " name=" << name << " mac=" << mac << std::endl;

    if (name != deviceConfig.name) {
        debug << "Another device, not interested." << std::endl;
        return;
    }

    if (mac != getMac() && available) {
        debug << "Device collision." << std::endl;
        client->disconnect();
        connectionBackoff();
    }

    if (availabilityReceiveTimeLimit != 0) {
        debug << "Got expected message." << std::endl;
        resetAvailabilityReceive();
        nextStatusSend = millis();
    } else if (!available) {
        debug << "Refreshing availability state." << std::endl;
        nextStatusSend = millis();
    }
}

void MqttClient::onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length) {
    receivedMessages.push_back(Message{topic,
            std::string{reinterpret_cast<const char*>(payload), length}});
}

void MqttClient::handleMessage(const Message& message) {
    debug << "Message received on topic " + message.topic << std::endl;
    if (message.topic == deviceConfig.availabilityTopic) {
        debug << message.payload << std::endl;
        DynamicJsonBuffer buffer(200);
        auto& json = buffer.parseObject(message.payload);
        handleAvailabilityMessage(json);
    }

    auto iterator = std::find_if(
            subscriptions.begin(), subscriptions.end(),
            [&message](const Subscription& element) {
                return element.first == message.topic;
            });
    if (iterator == subscriptions.end()) {
        debug << "No subscription for topic." << std::endl;
        return;
    }
    iterator->second(message.payload);
}

bool MqttClient::tryToConnect(const ServerConfig& server) {
    debug << "Connecting to " << server.address << ":" << server.port
        << " as " << server.username << std::endl;
    bool result = false;
    client->setServer(server.address.c_str(), server.port).setClient(*wifiClient)
            .setCallback([this](const char* topic, const unsigned char* payload, unsigned length) {
                onMessageReceived(topic, payload, length);
            });
    std::string clientId = deviceConfig.name + "-" + getMac();
    debug << "clientId=" + clientId << std::endl;
    if (deviceConfig.availabilityTopic.length() != 0) {
        willMessage = getStatusMessage(false, false);
        result = client->connect(
                clientId.c_str(),
                server.username.c_str(),
                server.password.c_str(),
                deviceConfig.availabilityTopic.c_str(), 0, true,
                willMessage.c_str());
        if (result) {
            debug << "Connected to server. Listening to availability topic."
                << std::endl;
            result = client->subscribe(deviceConfig.availabilityTopic.c_str());
            if (!result) {
                debug << "Failed to listen to availability topic." << std::endl;
                client->disconnect();
            }
            availabilityReceiveTimeLimit =
                    millis() + availabilityReceiveTimeout;
        }
    } else {
        result = client->connect(
                clientId.c_str(),
                server.username.c_str(),
                server.password.c_str());
    }
    return result;
}

MqttClient::ConnectStatus MqttClient::connectIfNeeded() {
    if (!client->connected()) {
        initialized = false;
        debug << "Client status = " + tools::intToString(client->state())
            << std::endl;
        debug << "Connecting to MQTT broker..." << std::endl;
        for (const ServerConfig& server : globalConfig.servers) {
            if (tryToConnect(server)) {
                debug << "Connection successful." << std::endl;
                for (const auto& element : subscriptions) {
                    client->subscribe(element.first.c_str());
                }
                break;
            }
        }

        if (!client->connected()) {
            debug << "Connection failed." << std::endl;
            return ConnectStatus::connectionFailed;
        }
    }

    if (availabilityReceiveTimeLimit != 0) {
        if (availabilityReceiveTimeLimit > millis()) {
            return ConnectStatus::connecting;
        }

        debug << "Did not get availability topic in time. "
                "Assuming we are first." << std::endl;
        resetAvailabilityReceive();
    }
    return ConnectStatus::connectionSuccessful;
}

void MqttClient::sendStatusMessage(bool restarted) {
    auto now = millis();
    if (deviceConfig.availabilityTopic.length() != 0 && now >= nextStatusSend) {
        debug << "Sending status message to topic "
                << deviceConfig.availabilityTopic << std::endl;
        std::string message = getStatusMessage(true, restarted);
        debug << message << std::endl;
        if (client->publish(deviceConfig.availabilityTopic.c_str(),
                message.c_str(), true)) {
            debug << "Success." << std::endl;
        } else {
            debug << "Failure." << std::endl;
        }
        nextStatusSend += ((now - nextStatusSend) / statusSendInterval + 1)
                * statusSendInterval;
    }
}


void MqttClient::loop() {
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

    client->loop();

    for (const auto& message : receivedMessages) {
        handleMessage(message);
    }
    receivedMessages.clear();
}

void MqttClient::subscribe(const std::string& topic,
        std::function<void(const std::string&)> callback) {
    subscriptions.emplace_back(topic, callback);
    if (client->connected()) {
        client->subscribe(topic.c_str());
    }
}

void MqttClient::unsubscribe(const std::string& topic) {
    subscriptions.erase(std::remove_if(
            subscriptions.begin(), subscriptions.end(),
            [&topic](const Subscription& element) {
                return element.first == topic;
            }), subscriptions.end());
    if (client->connected()) {
        client->unsubscribe(topic.c_str());
    }
}

void MqttClient::publish(
        const std::string& topic, const std::string& payload, bool retain) {
    if (!client->publish(topic.c_str(), payload.c_str(), retain)) {
        debug << "Publishing to " << topic << " failed." << std::endl;
    }
}

void MqttClient::disconnect() {
    client->disconnect();
}

bool MqttClient::isConnected() const {
    return client->connected();
}
