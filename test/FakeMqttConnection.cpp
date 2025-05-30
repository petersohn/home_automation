#include <boost/test/unit_test.hpp>

#include "FakeMqttConnection.hpp"

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
        wills.emplace(nextId, std::move(*will));
    }
    return nextId++;
}

void FakeMqttServer::disconnect(size_t id) {
    auto it = wills.find(id);
    if (it != wills.end()) {
        publish(id, it->second);
        wills.erase(it);
    }
}

void disconnect(size_t id);

bool FakeMqttServer::subscribe(
    size_t id, const std::string& topic,
    std::function<void(size_t, FakeMessage)> callback) {
    if (!subscriptions.emplace(std::make_pair(topic, id), callback).second) {
        return false;
    }
    auto it = retainedMessages.find(topic);
    if (it != retainedMessages.end()) {
        callback(it->second.first, it->second.second);
    }

    return true;
}

bool FakeMqttServer::unsubscribe(size_t id, const std::string& topic) {
    return subscriptions.erase(std::make_pair(topic, id)) != 0;
}

void FakeMqttServer::publish(size_t id, const FakeMessage& message) {
    BOOST_TEST_MESSAGE("publish " << message.topic << " " << message.payload);
    for (auto it = subscriptions.lower_bound(std::make_pair(message.topic, 0));
         it != subscriptions.end() && it->first.first == message.topic; ++it) {
        it->second(id, message);
    }

    if (message.retain) {
        if (message.payload.empty()) {
            retainedMessages.erase(message.topic);
        } else {
            retainedMessages[message.topic] = std::make_pair(id, message);
        }
    }
}

FakeMqttConnection::FakeMqttConnection(
    FakeMqttServer& server, std::function<void(bool)> connectCallback)
    : server(server), connectCallback(connectCallback) {}

bool FakeMqttConnection::connectInner(
    const std::optional<Message>& will, ReceiveHandler receiveFunc_) {
    if (connectionId || !server.working) {
        return false;
    }

    connectionId = server.connect(
        will ? FakeMessage{*will} : std::optional<FakeMessage>{});
    receiveFunc = std::move(receiveFunc_);
    return true;
}

bool FakeMqttConnection::connect(
    const char* /* host */, uint16_t /* port */, const char* /* username */,
    const char* /* password */, const char* /* clientId */,
    const std::optional<Message>& will, ReceiveHandler receiveFunc_) {
    auto result = connectInner(will, std::move(receiveFunc_));
    if (connectCallback) {
        connectCallback(result);
    }
    return result;
}

void FakeMqttConnection::disconnect() {
    if (connectionId) {
        server.disconnect(*connectionId);
        connectionId.reset();
    }
}

bool FakeMqttConnection::isConnected() {
    return connectionId.has_value();
}

bool FakeMqttConnection::subscribe(const char* topic) {
    if (!connectionId) {
        return false;
    }

    return server.subscribe(
        *connectionId, topic, [this](size_t /*id*/, FakeMessage message) {
        queue.emplace_back(std::move(message));
    });
}

bool FakeMqttConnection::unsubscribe(const char* topic) {
    if (!connectionId) {
        return false;
    }

    return server.unsubscribe(*connectionId, topic);
}

bool FakeMqttConnection::publish(const Message& message) {
    if (!connectionId) {
        return false;
    }

    server.publish(*connectionId, FakeMessage{message});
    return true;
}

void FakeMqttConnection::loop() {
    if (!connectionId || !receiveFunc) {
        return;
    }

    for (const auto& message : queue) {
        receiveFunc(message.toMessage());
    }
    queue.clear();
}
