#include "MqttClient.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../tools/string.hpp"
#include "Interface.hpp"

using namespace ArduinoJson;

namespace {

constexpr unsigned initialBackoff = 500;
constexpr unsigned maxBackoff = 60000;
constexpr unsigned statusSendInterval = 60000;
constexpr unsigned availabilityReceiveTimeout = 2000;
constexpr unsigned subscriptionRetryInterval = 1000;

int hexValue(unsigned char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

bool parseMac(const std::string& value, uint8_t (&bytes)[6]) {
    if (value.size() != 17) {
        return false;
    }

    for (size_t index = 0; index < 6; ++index) {
        const size_t offset = index * 3;
        const int high = hexValue(static_cast<unsigned char>(value[offset]));
        const int low = hexValue(static_cast<unsigned char>(value[offset + 1]));
        if (high < 0 || low < 0 || (index < 5 && value[offset + 2] != ':')) {
            return false;
        }
        bytes[index] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

bool sameMac(const uint8_t (&left)[6], const uint8_t (&right)[6]) {
    for (size_t index = 0; index < 6; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

const char* phaseName(UploadPhase phase) {
    switch (phase) {
    case UploadPhase::idle:
        return "idle";
    case UploadPhase::pending:
        return "pending";
    case UploadPhase::started:
        return "started";
    }
    return "idle";
}

}  // unnamed namespace

MqttClient::MqttClient(
    std::ostream& debug, EspApi& esp, Wifi& wifi, Backoff& backoff,
    MqttConnection& connection, CycleTimer& cycleTimer,
    std::function<void()> onConnected)
    : debug(debug)
    , esp(esp)
    , wifi(wifi)
    , backoff(backoff)
    , connection(connection)
    , cycleTimer(cycleTimer)
    , onConnected(std::move(onConnected))
    , initState(InitState::Begin)
    , currentBackoff(initialBackoff) {}

const char* MqttClient::getStatusMessage(bool restarted) {
    this->statusMsgBuf.clear();
    const auto fail = [this]() -> const char* {
        this->statusMsg[0] = '\0';
        return nullptr;
    };
    JsonObject& message = this->statusMsgBuf.createObject();
    if (!message.success()) {
        return fail();
    }
    if (!message.set("name", this->config.name.c_str()) ||
        !message.set("mac", this->wifi.getMac()) ||
        !message.set("restarted", restarted) ||
        !message.set("ip", this->wifi.getIp()) ||
        !message.set("uptime", this->esp.millis()) ||
        !message.set("freeMemory", this->esp.getFreeHeap())) {
        return fail();
    }

    float avgCycleTime = this->cycleTimer.getAvgCycleTime();
    if (!message.set("avgCycleTime", avgCycleTime) ||
        !message.set("maxCycleTime", this->cycleTimer.getMaxCycleTime())) {
        return fail();
    }

    size_t expectedFields = 8;
    if (this->otaStatusProvider) {
        const OtaStatusSnapshot snapshot = this->otaStatusProvider();
        if (!message.set("configVersion", snapshot.configVersion)) {
            return fail();
        }
        if (snapshot.minimumAcceptedConfigVersion.has_value()) {
            if (!message.set(
                    "minimumAcceptedConfigVersion",
                    *snapshot.minimumAcceptedConfigVersion)) {
                return fail();
            }
        } else {
            if (!message.set("minimumAcceptedConfigVersion", RawJson("null"))) {
                return fail();
            }
        }
        if (!message.set("trialState", phaseName(snapshot.phase))) {
            return fail();
        }
        if (snapshot.lastOutcome.has_value()) {
            JsonObject& result = message.createNestedObject("lastConfigUpdate");
            if (!result.success()) {
                return fail();
            }
            if (!result.set("status", snapshot.lastOutcome->status) ||
                !result.set("transferred", snapshot.lastOutcome->transferred) ||
                !result.set("hash", snapshot.lastOutcome->hash)) {
                return fail();
            }
            if (snapshot.lastOutcome->version.has_value()) {
                if (!result.set("version", *snapshot.lastOutcome->version)) {
                    return fail();
                }
            } else {
                if (!result.set("version", RawJson("null"))) {
                    return fail();
                }
            }
            if (!result.set("reason", snapshot.lastOutcome->reason) ||
                result.size() != 5) {
                return fail();
            }
        } else {
            if (!message.set("lastConfigUpdate", RawJson("null"))) {
                return fail();
            }
        }
        expectedFields += 4;
    }

    const size_t topicLength = this->config.topics.statusTopic.size();
    if (message.size() != expectedFields ||
        topicLength + mqttPacketHeaderSize >= mqttPacketSize) {
        return fail();
    }

    const size_t length = message.measureLength();
    const size_t payloadLimit =
        mqttPacketSize - topicLength - mqttPacketHeaderSize;
    if (length > payloadLimit || length >= sizeof(this->statusMsg)) {
        return fail();
    }
    const size_t written =
        message.printTo(this->statusMsg, sizeof(this->statusMsg));
    if (written != length || written > payloadLimit) {
        return fail();
    }
    return this->statusMsg;
}

void MqttClient::setConfig(MqttConfig config_) {
    this->config = std::move(config_);
}

void MqttClient::availabiltyReceiveSuccess() {
    this->availabilityReceiveTimeLimit = 0;
    this->initState = InitState::Done;
    if (this->readiness != MqttReadiness::collision) {
        this->readiness = MqttReadiness::ready;
    }
}

void MqttClient::availabiltyReceiveFail() {
    this->debug << "Device collision." << std::endl;
    this->disconnectTransport();
    this->connectionBackoff();
    this->availabilityReceiveTimeLimit = 0;
    this->initState = InitState::Begin;
    if (this->readiness != MqttReadiness::collision) {
        this->readiness = MqttReadiness::checking;
    }
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

    uint8_t receivedMac[6];
    uint8_t ownMac[6];
    if (!parseMac(mac, receivedMac) || !parseMac(this->wifi.getMac(), ownMac)) {
        this->debug << "Invalid device MAC." << std::endl;
        return;
    }

    if (!sameMac(receivedMac, ownMac)) {
        this->collisionLatched = true;
        this->readiness = MqttReadiness::collision;
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
    this->updateTransportState();
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
        memcpy(payload, message.payload, message.payloadLength);
        payload[message.payloadLength] = 0;
        StaticJsonBuffer<statusMsgBufSize> buffer;
        auto& json = buffer.parseObject(payload);
        if (!json.success()) {
            this->debug << "Invalid status message." << std::endl;
            return;
        }
        this->handleStatusMessage(json);
        return;
    }

    auto iterator = std::find_if(
        this->subscriptions.begin(), this->subscriptions.end(),
        [&message, this](const Subscription& element) {
        return element.priority &&
               strcmp(message.topic, element.topic.c_str()) == 0;
    });
    if (iterator == this->subscriptions.end()) {
        iterator = std::find_if(
            this->subscriptions.begin(), this->subscriptions.end(),
            [&message](const Subscription& element) {
            return !element.priority &&
                   strcmp(message.topic, element.topic.c_str()) == 0;
        });
    }
    if (iterator == this->subscriptions.end()) {
        this->debug << "No subscription for topic." << std::endl;
        return;
    }
    iterator->callback(message);
}

void MqttClient::updateTransportState() {
    bool connected = this->wifi.getStatus() == WifiStatus::Connected &&
                     this->connection.isConnected();
    if (this->transportState.connected && !connected) {
        ++this->transportState.generation;
        if (this->readiness != MqttReadiness::collision) {
            this->readiness = MqttReadiness::checking;
        }
    }
    this->transportState.connected = connected;
}

void MqttClient::disconnectTransport() {
    this->updateTransportState();
    bool wasConnected =
        this->transportState.connected || this->connection.isConnected();
    this->connection.disconnect();
    if (wasConnected) {
        ++this->transportState.generation;
    }
    this->transportState.connected = false;
    if (this->readiness != MqttReadiness::collision) {
        this->readiness = MqttReadiness::checking;
    }
}

bool MqttClient::tryToConnect(const ServerConfig& server) {
    this->readiness = MqttReadiness::checking;
    this->initState = InitState::Begin;
    this->availabilityReceiveTimeLimit = 0;
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

    ++this->transportState.generation;
    this->updateTransportState();

    if (hasAvailability) {
        this->debug << "Connected to server. Listening to availability topic."
                    << std::endl;
        result = this->connection.subscribe(
            this->config.topics.availabilityTopic.c_str());
        if (!result) {
            this->debug << "Failed to listen to availability topic."
                        << std::endl;
            this->disconnectTransport();
            return false;
        }

        if (this->config.topics.statusTopic.size() != 0) {
            result = this->connection.subscribe(
                this->config.topics.statusTopic.c_str());
            if (!result) {
                this->debug << "Failed to listen to status topic." << std::endl;
                this->disconnectTransport();
                return false;
            }
        }

        this->initState = InitState::Begin;
        this->availabilityReceiveTimeLimit =
            this->esp.millis() + availabilityReceiveTimeout;
    } else {
        this->initState = InitState::Done;
        this->readiness = MqttReadiness::ready;
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
                for (auto& element : this->subscriptions) {
                    this->attemptSubscription(element);
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
        if (message == nullptr) {
            this->debug << "Status message exceeds MQTT packet capacity."
                        << std::endl;
        } else {
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
    }

    this->nextStatusSend +=
        ((now - this->nextStatusSend) / statusSendInterval + 1) *
        statusSendInterval;
    this->cycleTimer.reset();
}

void MqttClient::loop() {
    auto now = this->esp.millis();
    this->updateTransportState();

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

    this->updateTransportState();
    this->connection.loop();
    this->updateTransportState();
    this->retrySubscriptions();
}

void MqttClient::connectedLoop() {}

void MqttClient::subscribe(
    const char* topic,
    std::function<void(const MqttConnection::Message&)> callback, uint8_t qos) {
    this->subscribeWithPriority(topic, std::move(callback), qos, false);
}

void MqttClient::subscribePriority(
    const char* topic,
    std::function<void(const MqttConnection::Message&)> callback, uint8_t qos) {
    this->subscribeWithPriority(topic, std::move(callback), qos, true);
}

void MqttClient::subscribeWithPriority(
    const char* topic,
    std::function<void(const MqttConnection::Message&)> callback, uint8_t qos,
    bool priority) {
    if (qos > 1) {
        this->debug << "Unsupported subscription QoS " << unsigned(qos) << "."
                    << std::endl;
        return;
    }
    this->debug << "Subscribing to " << topic << std::endl;
    this->subscriptions.push_back(
        Subscription{topic, std::move(callback), qos, priority});
    this->updateTransportState();
    if (this->transportState.connected) {
        this->attemptSubscription(this->subscriptions.back());
    }
}

void MqttClient::attemptSubscription(Subscription& subscription) {
    if (!this->transportState.connected || !this->connection.isConnected()) {
        return;
    }

    const auto prioritySubscription = std::find_if(
        this->subscriptions.begin(), this->subscriptions.end(),
        [&subscription](const Subscription& element) {
        return element.priority && element.topic == subscription.topic;
    });
    if (prioritySubscription != this->subscriptions.end() &&
        &*prioritySubscription != &subscription) {
        return;
    }

    const bool success = this->connection.subscribe(
        subscription.topic.c_str(), subscription.qos);
    subscription.successful = success;
    if (success) {
        subscription.successGeneration = this->transportState.generation;
        subscription.retryAt = 0;
    } else {
        subscription.retryAt = this->esp.millis() + subscriptionRetryInterval;
    }
}

void MqttClient::retrySubscriptions() {
    if (!this->transportState.connected) {
        return;
    }

    const unsigned long now = this->esp.millis();
    for (auto& subscription : this->subscriptions) {
        if (subscription.successful &&
            subscription.successGeneration == this->transportState.generation) {
            continue;
        }
        if (subscription.retryAt != 0 && now < subscription.retryAt) {
            continue;
        }
        this->attemptSubscription(subscription);
    }
}

void MqttClient::unsubscribe(const char* topic) {
    this->subscriptions.erase(
        std::remove_if(
            this->subscriptions.begin(), this->subscriptions.end(),
            [&topic](const Subscription& element) {
        return element.topic == topic;
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

bool MqttClient::canPublish() const {
    const_cast<MqttClient*>(this)->updateTransportState();
    return this->initialized && this->transportState.connected;
}

bool MqttClient::tryPublish(
    const char* topic, const char* payload, bool retain) {
    if (!this->canPublish()) {
        return false;
    }
    if (!this->connection.publish(
            MqttConnection::Message{topic, payload, strlen(payload), retain})) {
        this->debug << "Publishing to " << topic << " failed." << std::endl;
        return false;
    }
    return true;
}

void MqttClient::disconnect() {
    this->disconnectTransport();
}

bool MqttClient::isConnected() const {
    return this->connection.isConnected();
}

MqttReadiness MqttClient::getReadiness() const {
    const_cast<MqttClient*>(this)->updateTransportState();
    return this->readiness;
}

bool MqttClient::takeCollision() {
    const bool collision = this->collisionLatched;
    this->collisionLatched = false;
    return collision;
}

void MqttClient::setOtaStatusProvider(
    std::function<OtaStatusSnapshot()> provider) {
    this->otaStatusProvider = std::move(provider);
}

bool MqttClient::subscriptionReady(const char* topic) const {
    const_cast<MqttClient*>(this)->updateTransportState();
    if (!this->transportState.connected) {
        return false;
    }
    const auto prioritySubscription = std::find_if(
        this->subscriptions.begin(), this->subscriptions.end(),
        [topic](const Subscription& subscription) {
        return subscription.priority && subscription.topic == topic;
    });
    if (prioritySubscription != this->subscriptions.end()) {
        return prioritySubscription->successful &&
               prioritySubscription->successGeneration ==
                   this->transportState.generation;
    }
    for (const auto& subscription : this->subscriptions) {
        if (!subscription.priority && subscription.topic == topic &&
            subscription.successful &&
            subscription.successGeneration == this->transportState.generation) {
            return true;
        }
    }
    return false;
}

MqttTransportState MqttClient::getTransportState() {
    this->updateTransportState();
    return this->transportState;
}
