#include "FakeMqttConnection.hpp"

#include <boost/test/unit_test.hpp>

FakeMessage::FakeMessage() : retain(false) {}

FakeMessage::FakeMessage(std::string topic, std::string payload, bool retain)
    : topic(std::move(topic)), payload(std::move(payload)), retain(retain) {}

FakeMessage::FakeMessage(const MqttConnection::Message msg)
    : topic(msg.topic), payload(msg.payload), retain(msg.retain) {}

MqttConnection::Message FakeMessage::toMessage() const {
    return MqttConnection::Message{
        topic.c_str(), payload.c_str(), payload.size(), retain};
}

size_t FakeMqttServer::connect(std::optional<FakeMessage> will) {
    if (will) {
        this->wills.emplace(nextId, std::move(*will));
    }
    return this->nextId++;
}

void FakeMqttServer::disconnect(size_t id) {
    auto it = this->wills.find(id);
    if (it != this->wills.end()) {
        this->publish(id, it->second);
        this->wills.erase(it);
    }
}

void disconnect(size_t id);

bool FakeMqttServer::subscribe(
    size_t id, const std::string& topic,
    std::function<void(size_t, FakeMessage)> callback) {
    if (!this->subscriptions.emplace(std::make_pair(topic, id), callback)
             .second) {
        return false;
    }
    auto it = this->retainedMessages.find(topic);
    if (it != this->retainedMessages.end()) {
        callback(it->second.first, it->second.second);
    }

    return true;
}

bool FakeMqttServer::unsubscribe(size_t id, const std::string& topic) {
    return this->subscriptions.erase(std::make_pair(topic, id)) != 0;
}

void FakeMqttServer::publish(size_t id, const FakeMessage& message) {
    BOOST_TEST_MESSAGE("publish " << message.topic << " " << message.payload);
    for (auto it =
             this->subscriptions.lower_bound(std::make_pair(message.topic, 0));
         it != this->subscriptions.end() && it->first.first == message.topic;
         ++it) {
        it->second(id, message);
    }

    if (message.retain) {
        if (message.payload.empty()) {
            this->retainedMessages.erase(message.topic);
        } else {
            this->retainedMessages[message.topic] = std::make_pair(id, message);
        }
    }
}

FakeMqttConnection::FakeMqttConnection(
    FakeMqttServer& server, std::function<void(bool)> connectCallback)
    : server(server), connectCallback(connectCallback) {}

bool FakeMqttConnection::connectInner(
    const std::optional<Message>& will, ReceiveHandler receiveFunc_) {
    if (this->connectionId || !this->server.working) {
        return false;
    }

    this->connectionId = this->server.connect(
        will ? FakeMessage{*will} : std::optional<FakeMessage>{});
    this->receiveFunc = std::move(receiveFunc_);
    return true;
}

bool FakeMqttConnection::connect(
    const char* /* host */, uint16_t /* port */, const char* /* username */,
    const char* /* password */, const char* /* clientId */,
    const std::optional<Message>& will, ReceiveHandler receiveFunc_) {
    auto result = this->connectInner(will, std::move(receiveFunc_));
    if (this->connectCallback) {
        this->connectCallback(result);
    }
    return result;
}

void FakeMqttConnection::disconnect() {
    if (this->connectionId) {
        this->server.disconnect(*this->connectionId);
        this->connectionId.reset();
    }
}

bool FakeMqttConnection::isConnected() {
    return this->connectionId.has_value();
}

bool FakeMqttConnection::subscribe(const char* topic) {
    if (!this->connectionId) {
        return false;
    }

    return this->server.subscribe(
        *this->connectionId, topic, [this](size_t /*id*/, FakeMessage message) {
        this->queue.emplace_back(std::move(message));
    });
}

bool FakeMqttConnection::unsubscribe(const char* topic) {
    if (!this->connectionId) {
        return false;
    }

    return this->server.unsubscribe(*this->connectionId, topic);
}

bool FakeMqttConnection::publish(const Message& message) {
    if (!this->connectionId) {
        return false;
    }

    this->server.publish(*this->connectionId, FakeMessage{message});
    return true;
}

void FakeMqttConnection::loop() {
    if (!this->connectionId || !this->receiveFunc) {
        return;
    }

    for (const auto& message : this->queue) {
        this->receiveFunc(message.toMessage());
    }
    this->queue.clear();
}
