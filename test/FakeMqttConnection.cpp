#include "FakeMqttConnection.hpp"

size_t FakeMqttServer::connect(std::optional<MqttConnection::Message> will) {
    if (will) {
        wills.emplace(nextId, std::move(*will));
    }
    return nextId++;
}

void FakeMqttServer::disconnect(size_t id) {
    auto it = wills.find(id);
    if (it != wills.end()) {
        publish(it->second);
        wills.erase(it);
    }
}

void disconnect(size_t id);

bool FakeMqttServer::subscribe(
        size_t id, const std::string& topic,
        std::function<void(MqttConnection::Message)> callback) {
    if (!subscriptions.emplace(
            std::make_pair(topic, id), std::move(callback)).second) {
        return false;
    }
    auto it = retainedMessages.find(topic);
    if (it != retainedMessages.end()) {
        callback(it->second);
    }

    return true;
}

bool FakeMqttServer::unsubscribe(size_t id, const std::string& topic) {
    return subscriptions.erase(std::make_pair(topic, id)) != 0;
}

void FakeMqttServer::publish(const MqttConnection::Message& message) {
    for (auto it = subscriptions.lower_bound(std::make_pair(message.topic, 0));
            it != subscriptions.end() && it->first.first == message.topic;
            ++it) {
        it->second(message);
    }

    if (message.retain) {
        retainedMessages[message.topic] = message;
    } else {
        retainedMessages.erase(message.topic);
    }
}

bool FakeMqttConnection::connect(
        const std::string& /* host */, uint16_t /* port */,
        const std::string& /* username */, const std::string& /* password */,
        const std::string& /* clientId */,
        const std::optional<Message>& will,
        std::function<void(Message)> receiveFunc_) {
    if (connectionId) {
        return false;
    }

    connectionId = server.connect(std::move(will));
    receiveFunc = std::move(receiveFunc_);
    return true;
}

void FakeMqttConnection::disconnect() {
    connectionId.reset();
}

bool FakeMqttConnection::isConnected() {
    return connectionId.has_value();
}

bool FakeMqttConnection::subscribe(const std::string& topic) {
    if (!connectionId) {
        return false;
    }

    return server.subscribe(*connectionId, topic, [this](Message message) {
            queue.emplace_back(std::move(message));
        });
}

bool FakeMqttConnection::unsubscribe(const std::string& topic) {
    if (!connectionId) {
        return false;
    }

    return server.unsubscribe(*connectionId, topic);
}

bool FakeMqttConnection::publish(const Message& message) {
    if (!connectionId) {
        return false;
    }

    server.publish(message);
    return true;
}

void FakeMqttConnection::loop() {
    if (!connectionId || !receiveFunc) {
        return;
    }

    for (auto& message : queue) {
        receiveFunc(std::move(message));
    }
    queue.clear();
}
