#include "MqttClient.hpp"

#include <algorithm>
#include <vector>

#include "../tools/string.hpp"
#include "Interface.hpp"

using namespace ArduinoJson;

namespace {

constexpr unsigned initialBackoff = 500;
constexpr unsigned maxBackoff = 60000;
constexpr unsigned statusSendInterval = 60000;
constexpr unsigned availabilityReceiveTimeout = 2000;

}  // unnamed namespace

MqttClient::MqttClient(
    std::ostream& debug, EspApi& esp, Wifi& wifi, Backoff& backoff,
    MqttConnection& connection, std::function<void()> onConnected)
    : debug(debug)
    , esp(esp)
    , wifi(wifi)
    , backoff(backoff)
    , connection(connection)
    , onConnected(std::move(onConnected))
    , initState(InitState::Begin)
    , currentBackoff(initialBackoff) {}

const char* MqttClient::getStatusMessage(bool restarted) {
    this->statusMsgBuf.clear();
    JsonObject& message = this->statusMsgBuf.createObject();
    message["name"] = this->config.name.c_str();
    message["mac"] = this->wifi.getMac();
    message["restarted"] = restarted;
    message["ip"] = this->wifi.getIp();
    message["uptime"] = this->esp.millis();
    message["freeMemory"] = this->esp.getFreeHeap();

    auto time = this->esp.millis() - this->previousStatusSend;
    float avgCycleTime = static_cast<float>(time) / this->cycles;
    message["avgCycleTime"] = avgCycleTime;
    message["maxCycleTime"] = this->maxCycleTime;

    message.printTo(this->statusMsg);
    return this->statusMsg;
}

void MqttClient::setConfig(MqttConfig config_) {
    this->config = std::move(config_);
}

void MqttClient::availabiltyReceiveSuccess() {
    this->availabilityReceiveTimeLimit = 0;
    this->initState = InitState::Done;
}

void MqttClient::availabiltyReceiveFail() {
    this->debug << "Device collision." << std::endl;
    this->connection.disconnect();
    this->connectionBackoff();
    this->availabilityReceiveTimeLimit = 0;
    this->initState = InitState::Begin;
}

void MqttClient::connectionBackoff() {
    this->nextConnectionAttempt = this->esp.millis() + this->currentBackoff;
    this->currentBackoff = std::min(this->currentBackoff * 2, maxBackoff);
}

void MqttClient::resetConnectionBackoff() {
    this->currentBackoff = initialBackoff;
}

void MqttClient::refreshAvailability() {
    this->debug << "Refreshing availability." << std::endl;
    this->nextStatusSend = this->esp.millis();
}

void MqttClient::handleStatusMessage(const JsonObject& message) {
    auto name = message.get<std::string>("name");
    auto mac = message.get<std::string>("mac");

    this->debug << "Got status: state=" << this->currentStateDebug()
                << " name=" << name << " mac=" << mac << std::endl;

    if (name != this->config.name) {
        this->debug << "Another device, not interested." << std::endl;
        return;
    }

    if (mac != this->wifi.getMac()) {
        switch (this->initState) {
        case InitState::ReceivedAvailable:
            this->availabiltyReceiveFail();
            break;
        case InitState::Done:
            this->refreshAvailability();
            break;
        case InitState::Begin:
            this->initState = InitState::ReceivedOtherDevice;
            break;
        case InitState::ReceivedOtherDevice:
            break;
        }
    } else if (this->initState != InitState::Done) {
        this->availabiltyReceiveSuccess();
    }
}

void MqttClient::handleAvailabilityMessage(bool available) {
    this->debug << "Got availability state=" << this->currentStateDebug()
                << " available=" << available << std::endl;
    if (available) {
        switch (this->initState) {
        case InitState::Begin:
            if (this->config.topics.statusTopic.size() != 0) {
                this->initState = InitState::ReceivedAvailable;
            } else {
                this->availabiltyReceiveFail();
            }
            break;
        case InitState::ReceivedOtherDevice:
            this->availabiltyReceiveFail();
            break;
        case InitState::ReceivedAvailable:
        case InitState::Done:
            break;
        }
    } else {
        if (this->initState == InitState::Done) {
            this->refreshAvailability();
        } else {
            this->availabiltyReceiveSuccess();
        }
    }
}

const char* MqttClient::currentStateDebug() const {
    switch (this->initState) {
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
    this->debug << "Message received on topic " << message.topic << std::endl;
    if (strcmp(message.topic, this->config.topics.availabilityTopic.c_str()) ==
        0) {
        bool isAvailable = false;
        if (tools::getBoolValue(
                message.payload, isAvailable, message.payloadLength)) {
            this->handleAvailabilityMessage(isAvailable);
        }
        return;
    }

    if (strcmp(message.topic, this->config.topics.statusTopic.c_str()) == 0) {
        if (message.payloadLength > statusMsgSize) {
            this->debug << "Invalid status message." << std::endl;
            return;
        }

        char payload[statusMsgSize + 1];
        strncpy(payload, message.payload, message.payloadLength);
        payload[message.payloadLength] = 0;
        StaticJsonBuffer<400> buffer;
        auto& json = buffer.parseObject(payload);
        this->handleStatusMessage(json);
        return;
    }

    auto iterator = std::find_if(
        this->subscriptions.begin(), this->subscriptions.end(),
        [&message, this](const Subscription& element) {
        return strcmp(message.topic, element.first.c_str()) == 0;
    });
    if (iterator == this->subscriptions.end()) {
        this->debug << "No subscription for topic." << std::endl;
        return;
    }
    iterator->second(message);
}

bool MqttClient::tryToConnect(const ServerConfig& server) {
    this->debug << "Connecting to " << server.address << ":" << server.port
                << " as " << server.username << std::endl;
    std::string clientId = this->config.name + "-" + this->wifi.getMac();
    this->debug << "clientId=" + clientId << std::endl;

    std::optional<MqttConnection::Message> will;
    bool hasAvailability = this->config.topics.availabilityTopic.length() != 0;
    if (hasAvailability) {
        will = MqttConnection::Message{
            this->config.topics.availabilityTopic.c_str(), "0", 1, true};
    }

    bool result = this->connection.connect(
        server.address.c_str(), server.port, server.username.c_str(),
        server.password.c_str(), clientId.c_str(), will,
        [this](const MqttConnection::Message& message) {
        this->handleMessage(message);
    });

    if (!result) {
        return false;
    }

    if (hasAvailability) {
        this->debug << "Connected to server. Listening to availability topic."
                    << std::endl;
        result = this->connection.subscribe(
            this->config.topics.availabilityTopic.c_str());
        if (!result) {
            this->debug << "Failed to listen to availability topic."
                        << std::endl;
            this->connection.disconnect();
            return false;
        }

        if (this->config.topics.statusTopic.size() != 0) {
            result = this->connection.subscribe(
                this->config.topics.statusTopic.c_str());
            if (!result) {
                this->debug << "Failed to listen to status topic." << std::endl;
                this->connection.disconnect();
                return false;
            }
        }

        this->initState = InitState::Begin;
        this->availabilityReceiveTimeLimit =
            this->esp.millis() + availabilityReceiveTimeout;
    } else {
        this->initState = InitState::Done;
    }

    return result;
}

MqttClient::ConnectStatus MqttClient::connectIfNeeded() {
    if (!this->connection.isConnected()) {
        this->initialized = false;
        this->debug << "Connecting to MQTT broker..." << std::endl;
        for (const ServerConfig& server : this->config.servers) {
            if (this->tryToConnect(server)) {
                this->debug << "Connection successful." << std::endl;
                for (const auto& element : this->subscriptions) {
                    this->connection.subscribe(element.first.c_str());
                }
                break;
            }
        }

        if (!this->connection.isConnected()) {
            this->debug << "Connection failed." << std::endl;
            return ConnectStatus::connectionFailed;
        }
    }

    if (this->initState != InitState::Done) {
        if (this->availabilityReceiveTimeLimit > this->esp.millis()) {
            return ConnectStatus::connecting;
        }

        this->debug << "Did not get availability topic in time. "
                       "Assuming we are first."
                    << std::endl;
        this->availabiltyReceiveSuccess();
    }
    return ConnectStatus::connectionSuccessful;
}

void MqttClient::sendStatusMessage(bool restarted) {
    auto now = this->esp.millis();
    if (now < this->nextStatusSend) {
        return;
    }

    if (this->config.topics.availabilityTopic.length() != 0) {
        this->debug << "Sending availability message to topic "
                    << this->config.topics.availabilityTopic << std::endl;
        if (this->connection.publish(
                MqttConnection::Message{
                    this->config.topics.availabilityTopic.c_str(), "1", 1,
                    true})) {
            this->debug << "Success." << std::endl;
        } else {
            this->debug << "Failure." << std::endl;
        }
    }

    if (this->config.topics.statusTopic.length() != 0) {
        this->debug << "Sending status message to topic "
                    << this->config.topics.statusTopic << std::endl;
        const char* message = this->getStatusMessage(restarted);
        this->debug << message << std::endl;
        if (this->connection.publish(
                MqttConnection::Message{
                    this->config.topics.statusTopic.c_str(), message,
                    0 /*not used*/, true})) {
            this->debug << "Success." << std::endl;
        } else {
            this->debug << "Failure." << std::endl;
        }
    }

    this->nextStatusSend +=
        ((now - this->nextStatusSend) / statusSendInterval + 1) *
        statusSendInterval;
    this->previousStatusSend = now;
    this->cycles = 0;
    this->maxCycleTime = 0;
}

void MqttClient::loop() {
    auto now = this->esp.millis();
    ++this->cycles;
    auto cycleTime = now - this->previousCycle;
    this->maxCycleTime = std::max(this->maxCycleTime, cycleTime);

    if (now >= this->nextConnectionAttempt) {
        switch (this->connectIfNeeded()) {
        case ConnectStatus::connectionSuccessful:
            this->backoff.good();
            if (this->initialized) {
                this->sendStatusMessage(false);
            } else {
                this->initialized = true;
                if (this->onConnected) {
                    this->onConnected();
                }
                this->nextStatusSend = now;
                this->sendStatusMessage(this->restarted);
                this->restarted = false;
                this->resetConnectionBackoff();
            }
            break;
        case ConnectStatus::connectionFailed:
            this->backoff.bad();
            this->connectionBackoff();
            break;
        case ConnectStatus::connecting:
            this->backoff.good();
            break;
        }
    }

    this->connection.loop();
    this->previousCycle = now;
}

void MqttClient::connectedLoop() {}

void MqttClient::subscribe(
    const char* topic,
    std::function<void(const MqttConnection::Message&)> callback) {
    this->debug << "Subscribing to " << topic << std::endl;
    this->subscriptions.emplace_back(topic, callback);
    if (this->connection.isConnected()) {
        this->connection.subscribe(topic);
    }
}

void MqttClient::unsubscribe(const char* topic) {
    this->subscriptions.erase(
        std::remove_if(
            this->subscriptions.begin(), this->subscriptions.end(),
            [&topic](const Subscription& element) {
        return element.first == topic;
    }),
        this->subscriptions.end());
    if (this->connection.isConnected()) {
        this->connection.unsubscribe(topic);
    }
}

void MqttClient::publish(const char* topic, const char* payload, bool retain) {
    if (!this->initialized) {
        return;
    }
    if (!this->connection.publish(
            MqttConnection::Message{topic, payload, strlen(payload), retain})) {
        this->debug << "Publishing to " << topic << " failed." << std::endl;
    }
}

void MqttClient::disconnect() {
    this->connection.disconnect();
}

bool MqttClient::isConnected() const {
    return this->connection.isConnected();
}
