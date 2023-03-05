#include "MqttClient.hpp"

#include "Interface.hpp"

#include "../tools/string.hpp"

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
    , connection(connection)
    , onConnected(std::move(onConnected))
    , initState(InitState::Begin)
    , currentBackoff(initialBackoff)
{
}

const char* MqttClient::getStatusMessage(bool restarted) {
    statusMsgBuf.clear();
    JsonObject& message = statusMsgBuf.createObject();
    message["name"] = config.name.c_str();
    message["mac"] = wifi.getMac();
    message["restarted"] = restarted;
    message["ip"] = wifi.getIp();
    message["uptime"] = esp.millis();
    message["freeMemory"] = esp.getFreeHeap();
    message.printTo(statusMsg);
    return statusMsg;
}

void MqttClient::setConfig(MqttConfig config_) {
    config = std::move(config_);
}

void MqttClient::availabiltyReceiveSuccess() {
    availabilityReceiveTimeLimit = 0;
    initState = InitState::Done;
}

void MqttClient::availabiltyReceiveFail() {
    debug << "Device collision." << std::endl;
    connection.disconnect();
    connectionBackoff();
    availabilityReceiveTimeLimit = 0;
    initState = InitState::Begin;
}

void MqttClient::connectionBackoff() {
    nextConnectionAttempt = esp.millis() + currentBackoff;
    currentBackoff = std::min(currentBackoff * 2, maxBackoff);
}

void MqttClient::resetConnectionBackoff() {
    currentBackoff = initialBackoff;
}

void MqttClient::refreshAvailability() {
    debug << "Refreshing availability." << std::endl;
    nextStatusSend = esp.millis();
}

void MqttClient::handleStatusMessage(const JsonObject& message) {
    auto name = message.get<std::string>("name");
    auto mac = message.get<std::string>("mac");

    debug << "Got status: state=" << currentStateDebug() << " name=" << name
            << " mac=" << mac << std::endl;

    if (name != config.name) {
        debug << "Another device, not interested." << std::endl;
        return;
    }

    if (mac != wifi.getMac()) {
        switch (initState) {
        case InitState::ReceivedAvailable:
            availabiltyReceiveFail();
            break;
        case InitState::Done:
            refreshAvailability();
            break;
        case InitState::Begin:
            initState = InitState::ReceivedOtherDevice;
            break;
        case InitState::ReceivedOtherDevice:
            break;
        }
    } else if (initState != InitState::Done) {
        availabiltyReceiveSuccess();
    }
}

void MqttClient::handleAvailabilityMessage(bool available)
{
    debug << "Got availability state=" << currentStateDebug() <<
            " available=" << available << std::endl;
    if (available) {
        switch (initState) {
        case InitState::Begin:
            if (config.topics.statusTopic.size() != 0) {
                initState = InitState::ReceivedAvailable;
            } else {
                availabiltyReceiveFail();
            }
            break;
        case InitState::ReceivedOtherDevice:
            availabiltyReceiveFail();
            break;
        case InitState::ReceivedAvailable:
        case InitState::Done:
            break;
        }
    } else {
        if (initState == InitState::Done) {
            refreshAvailability();
        } else {
            availabiltyReceiveSuccess();
        }
    }
}

const char* MqttClient::currentStateDebug() const
{
    switch (initState) {
    case InitState::Begin:
        return "Begin";
    case InitState::ReceivedAvailable:
        return "ReceivedAvailable";
    case InitState::ReceivedOtherDevice:
        return "ReceivedOtherDevice";
    case InitState::Done:
        return "Done";
    }

    return "Invalid";
}

void MqttClient::handleMessage(const MqttConnection::Message& message) {
    debug << "Message received on topic " << message.topic << std::endl;
    if (strcmp(message.topic, config.topics.availabilityTopic.c_str()) == 0) {
        bool isAvailable = false;
        if (tools::getBoolValue(
                message.payload, isAvailable, message.payloadLength)) {
            handleAvailabilityMessage(isAvailable);
        }
        return;
    }

    if (strcmp(message.topic, config.topics.statusTopic.c_str()) == 0) {
        if (message.payloadLength > statusMsgSize) {
            debug << "Invalid status message." << std::endl;
            return;
        }

        char payload[statusMsgSize + 1];
        strncpy(payload, message.payload, message.payloadLength);
        payload[message.payloadLength] = 0;
        StaticJsonBuffer<400> buffer;
        auto& json = buffer.parseObject(payload);
        handleStatusMessage(json);
        return;
    }

    auto iterator = std::find_if(
            subscriptions.begin(), subscriptions.end(),
            [&message, this](const Subscription& element) {
                return strcmp(message.topic, element.first.c_str()) == 0;
            });
    if (iterator == subscriptions.end()) {
        debug << "No subscription for topic." << std::endl;
        return;
    }
    iterator->second(message);
}

bool MqttClient::tryToConnect(const ServerConfig& server) {
    debug << "Connecting to " << server.address << ":" << server.port
        << " as " << server.username << std::endl;
    std::string clientId = config.name + "-" + wifi.getMac();
    debug << "clientId=" + clientId << std::endl;

    std::optional<MqttConnection::Message> will;
    bool hasAvailability = config.topics.availabilityTopic.length() != 0;
    if (hasAvailability) {
        will = MqttConnection::Message{
            config.topics.availabilityTopic.c_str(), "0", 1, true};
    }

    bool result = connection.connect(
            server.address.c_str(), server.port,
            server.username.c_str(), server.password.c_str(), clientId.c_str(),
            will,
            [this](const MqttConnection::Message& message) {
                handleMessage(message);
            });

    if (!result) {
        return false;
    }

    if (hasAvailability) {
        debug << "Connected to server. Listening to availability topic."
            << std::endl;
        result = connection.subscribe(config.topics.availabilityTopic.c_str());
        if (!result) {
            debug << "Failed to listen to availability topic." << std::endl;
            connection.disconnect();
            return false;
        }

        if (config.topics.statusTopic.size() != 0) {
            result = connection.subscribe(config.topics.statusTopic.c_str());
            if (!result) {
                debug << "Failed to listen to status topic." << std::endl;
                connection.disconnect();
                return false;
            }
        }

        initState = InitState::Begin;
        availabilityReceiveTimeLimit =
                esp.millis() + availabilityReceiveTimeout;
    } else {
        initState = InitState::Done;
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
                    connection.subscribe(element.first.c_str());
                }
                break;
            }
        }

        if (!connection.isConnected()) {
            debug << "Connection failed." << std::endl;
            return ConnectStatus::connectionFailed;
        }
    }

    if (initState != InitState::Done) {
        if (availabilityReceiveTimeLimit > esp.millis()) {
            return ConnectStatus::connecting;
        }

        debug << "Did not get availability topic in time. "
                "Assuming we are first." << std::endl;
        availabiltyReceiveSuccess();
    }
    return ConnectStatus::connectionSuccessful;
}

void MqttClient::sendStatusMessage(bool restarted) {
    auto now = esp.millis();
    if (now < nextStatusSend) {
        return;
    }

    if (config.topics.availabilityTopic.length() != 0) {
        debug << "Sending availability message to topic "
                << config.topics.availabilityTopic << std::endl;
        if (connection.publish(MqttConnection::Message{
                config.topics.availabilityTopic.c_str(), "1", 1, true})) {
            debug << "Success." << std::endl;
        } else {
            debug << "Failure." << std::endl;
        }
    }

    if (config.topics.statusTopic.length() != 0) {
        debug << "Sending status message to topic "
                << config.topics.statusTopic << std::endl;
        const char* message = getStatusMessage(restarted);
        debug << message << std::endl;
        if (connection.publish(MqttConnection::Message{
                config.topics.statusTopic.c_str(), message,
                0 /*not used*/, true})) {
            debug << "Success." << std::endl;
        } else {
            debug << "Failure." << std::endl;
        }
    }

    nextStatusSend += ((now - nextStatusSend) / statusSendInterval + 1)
            * statusSendInterval;
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
            backoff.good();
            break;
        }
    }

    connection.loop();
}

void MqttClient::subscribe(const char* topic,
        std::function<void(const MqttConnection::Message&)> callback) {
    subscriptions.emplace_back(topic, callback);
    if (connection.isConnected()) {
        connection.subscribe(topic);
    }
}

void MqttClient::unsubscribe(const char* topic) {
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
        const char* topic, const char* payload, bool retain) {
    if (!initialized) {
        return;
    }
    if (!connection.publish(MqttConnection::Message{topic, payload,
            strlen(payload), retain})) {
        debug << "Publishing to " << topic << " failed." << std::endl;
    }
}

void MqttClient::disconnect() {
    connection.disconnect();
}

bool MqttClient::isConnected() const {
    return connection.isConnected();
}
