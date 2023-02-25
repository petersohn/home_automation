#include "MqttClient.hpp"

#include "ArduinoJson.hpp"
#include "Interface.hpp"

#include <algorithm>
#include <vector>

using namespace ArduinoJson;

namespace {

constexpr unsigned initialBackoff = 500;
constexpr unsigned maxBackoff = 60000;
constexpr unsigned statusSendInterval = 60000;
constexpr unsigned availabilityReceiveTimeout = 2000;

} // unnamed namespace

MqttClient::MqttClient(std::ostream& debug, EspApi& esp, Wifi& wifi,
        Backoff& backoff, MqttConnection& connection,
        std::function<void()> onConnected)
    : debug(debug)
    , esp(esp)
    , wifi(wifi)
    , backoff(backoff)
    , currentBackoff(initialBackoff)
    , connection(connection)
    , onConnected(std::move(onConnected))
{
}

std::string MqttClient::getStatusMessage(bool available, bool restarted) {
    DynamicJsonBuffer buffer(200);
    JsonObject& message = buffer.createObject();
    message["name"] = config.name.c_str();
    message["available"] = available;
    message["mac"] = wifi.getMac();
    if (available) {
        message["restarted"] = restarted;
        message["ip"] = wifi.getIp();
        message["uptime"] = esp.millis();
        message["freeMemory"] = esp.getFreeHeap();
    }
    std::string result;
    message.printTo(result);
    return result;
}

void MqttClient::setConfig(MqttConfig config_) {
    config = std::move(config_);
}

void MqttClient::resetAvailabilityReceive() {
    availabilityReceiveTimeLimit = 0;
}

void MqttClient::connectionBackoff() {
    nextConnectionAttempt = esp.millis() + currentBackoff;
    currentBackoff = std::min(currentBackoff * 2, maxBackoff);
}

void MqttClient::resetConnectionBackoff() {
    currentBackoff = initialBackoff;
}

void MqttClient::handleAvailabilityMessage(const JsonObject& message) {
    bool available = message.get<bool>("available");
    auto name = message.get<std::string>("name");
    auto mac = message.get<std::string>("mac");

    debug << "Got availability: available=" << available
        << " name=" << name << " mac=" << mac << std::endl;

    if (name != config.name) {
        debug << "Another device, not interested." << std::endl;
        return;
    }

    if (mac != wifi.getMac() && available) {
        debug << "Device collision." << std::endl;
        connection.disconnect();
        connectionBackoff();
    }

    if (availabilityReceiveTimeLimit != 0) {
        debug << "Got expected message." << std::endl;
        resetAvailabilityReceive();
        nextStatusSend = esp.millis();
    } else if (!available) {
        debug << "Refreshing availability state." << std::endl;
        nextStatusSend = esp.millis();
    }
}

void MqttClient::onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length) {
}

void MqttClient::handleMessage(const MqttConnection::Message& message) {
    debug << "Message received on topic " + message.topic << std::endl;
    if (message.topic == config.topics.availabilityTopic) {
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
    std::string clientId = config.name + "-" + wifi.getMac();
    debug << "clientId=" + clientId << std::endl;

    std::optional<MqttConnection::Message> will;
    bool hasAvailability = config.topics.availabilityTopic.length() != 0;
    if (hasAvailability) {
        willMessage = getStatusMessage(false, false);
        will = MqttConnection::Message{
            config.topics.availabilityTopic, willMessage, true};
    }

    bool result = connection.connect(
            server.address, server.port,
            server.username, server.password, clientId, will,
            [this](MqttConnection::Message message) {
                receivedMessages.emplace_back(std::move(message));
            });

    if (hasAvailability && result) {
        debug << "Connected to server. Listening to availability topic."
            << std::endl;
        result = connection.subscribe(config.topics.availabilityTopic);
        if (!result) {
            debug << "Failed to listen to availability topic." << std::endl;
            connection.disconnect();
        }
        availabilityReceiveTimeLimit =
                esp.millis() + availabilityReceiveTimeout;
    }

    return result;
}

MqttClient::ConnectStatus MqttClient::connectIfNeeded() {
    if (!connection.isConnected()) {
        initialized = false;
        debug << "Connecting to MQTT broker..." << std::endl;
        for (const ServerConfig& server : config.servers) {
            if (tryToConnect(server)) {
                debug << "Connection successful." << std::endl;
                for (const auto& element : subscriptions) {
                    connection.subscribe(element.first);
                }
                break;
            }
        }

        if (!connection.isConnected()) {
            debug << "Connection failed." << std::endl;
            return ConnectStatus::connectionFailed;
        }
    }

    if (availabilityReceiveTimeLimit != 0) {
        if (availabilityReceiveTimeLimit > esp.millis()) {
            return ConnectStatus::connecting;
        }

        debug << "Did not get availability topic in time. "
                "Assuming we are first." << std::endl;
        resetAvailabilityReceive();
    }
    return ConnectStatus::connectionSuccessful;
}

void MqttClient::sendStatusMessage(bool restarted) {
    auto now = esp.millis();
    if (config.topics.availabilityTopic.length() != 0 && now >= nextStatusSend) {
        debug << "Sending status message to topic "
                << config.topics.availabilityTopic << std::endl;
        std::string message = getStatusMessage(true, restarted);
        debug << message << std::endl;
        if (connection.publish(MqttConnection::Message{
                config.topics.availabilityTopic, message, true})) {
            debug << "Success." << std::endl;
        } else {
            debug << "Failure." << std::endl;
        }
        nextStatusSend += ((now - nextStatusSend) / statusSendInterval + 1)
                * statusSendInterval;
    }
}


void MqttClient::loop() {
    if (esp.millis() >= nextConnectionAttempt) {
        switch (connectIfNeeded()) {
        case ConnectStatus::connectionSuccessful:
            backoff.good();
            if (initialized) {
                sendStatusMessage(false);
            } else {
                initialized = true;
                if (onConnected) {
                    onConnected();
                }
                nextStatusSend = esp.millis();
                sendStatusMessage(restarted);
                restarted = false;
                resetConnectionBackoff();
            }
            break;
        case ConnectStatus::connectionFailed:
            backoff.bad();
            connectionBackoff();
            break;
        case ConnectStatus::connecting:
            break;
        }
    }

    connection.loop();

    for (const auto& message : receivedMessages) {
        handleMessage(message);
    }
    receivedMessages.clear();
}

void MqttClient::subscribe(const std::string& topic,
        std::function<void(const std::string&)> callback) {
    subscriptions.emplace_back(topic, callback);
    if (connection.isConnected()) {
        connection.subscribe(topic);
    }
}

void MqttClient::unsubscribe(const std::string& topic) {
    subscriptions.erase(std::remove_if(
            subscriptions.begin(), subscriptions.end(),
            [&topic](const Subscription& element) {
                return element.first == topic;
            }), subscriptions.end());
    if (connection.isConnected()) {
        connection.unsubscribe(topic);
    }
}

void MqttClient::publish(
        const std::string& topic, const std::string& payload, bool retain) {
    if (!initialized) {
        return;
    }
    if (!connection.publish(MqttConnection::Message{topic, payload, retain})) {
        debug << "Publishing to " << topic << " failed." << std::endl;
    }
}

void MqttClient::disconnect() {
    connection.disconnect();
}

bool MqttClient::isConnected() const {
    return connection.isConnected();
}
