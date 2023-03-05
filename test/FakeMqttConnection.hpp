#ifndef TEST_FAKEMQTTCONNECTION_HPP
#define TEST_FAKEMQTTCONNECTION_HPP

#include "common/MqttConnection.hpp"

#include <map>
#include <string>
#include <optional>

struct FakeMessage {
    std::string topic;
    std::string payload;
    bool retain;

    FakeMessage();
    FakeMessage(std::string topic, std::string payload, bool retain = false);
    explicit FakeMessage(const MqttConnection::Message msg);

    FakeMessage(const FakeMessage&) = default;
    FakeMessage& operator=(const FakeMessage&) = default;
    FakeMessage(FakeMessage&&) = default;
    FakeMessage& operator=(FakeMessage&&) = default;

    MqttConnection::Message toMessage() const;
};

class FakeMqttServer {
public:
    size_t connect(std::optional<FakeMessage> will);
    void disconnect(size_t id);
    bool subscribe(
            size_t id, const std::string& topic,
            std::function<void(size_t, FakeMessage)> callback);
    bool unsubscribe(size_t id, const std::string& topic);
    void publish(size_t id, const FakeMessage& message);

    bool working = true;

private:
    size_t nextId = 0;
    std::map<
        std::pair<std::string, size_t>,
        std::function<void(size_t, FakeMessage)>> subscriptions;
    std::map<std::string, std::pair<size_t, FakeMessage>>
        retainedMessages;
    std::map<size_t, FakeMessage> wills;
};

class FakeMqttConnection: public MqttConnection {
public:
    FakeMqttConnection(FakeMqttServer& server,
            std::function<void(bool)> connectCallback);
    virtual bool connect(const char* host, uint16_t port,
            const char* username, const char* password,
            const char* clientId,
            const std::optional<Message>& will,
            ReceiveHandler receiveFunc) override;
    virtual void disconnect() override;
    virtual bool isConnected() override;

    virtual bool subscribe(const char* topic) override;
    virtual bool unsubscribe(const char* topic) override;
    virtual bool publish(const Message& message) override;
    virtual void loop() override;

private:
    FakeMqttServer& server;
    std::function<void(bool)> connectCallback;
    std::optional<size_t> connectionId;
    ReceiveHandler receiveFunc;
    std::vector<FakeMessage> queue;

    bool connectInner(const std::optional<Message>& will,
        ReceiveHandler receiveFunc);
};


#endif // TEST_FAKEMQTTCONNECTION_HPP
