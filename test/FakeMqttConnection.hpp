#ifndef TEST_FAKEMQTTCONNECTION_HPP
#define TEST_FAKEMQTTCONNECTION_HPP

#include "common/MqttConnection.hpp"

#include <map>
#include <optional>

class FakeMqttServer {
public:
    size_t connect(std::optional<MqttConnection::Message> will);
    void disconnect(size_t id);
    bool subscribe(
            size_t id, const std::string& topic,
            std::function<void(size_t, MqttConnection::Message)> callback);
    bool unsubscribe(size_t id, const std::string& topic);
    void publish(size_t id, const MqttConnection::Message& message);

    bool working = true;

private:
    size_t nextId = 0;
    std::map<
        std::pair<std::string, size_t>,
        std::function<void(size_t, MqttConnection::Message)>> subscriptions;
    std::map<std::string, std::pair<size_t, MqttConnection::Message>>
        retainedMessages;
    std::map<size_t, MqttConnection::Message> wills;
};

class FakeMqttConnection: public MqttConnection {
public:
    FakeMqttConnection(FakeMqttServer& server,
            std::function<void(bool)> connectCallback);
    virtual bool connect(const std::string& host, uint16_t port,
            const std::string& username, const std::string& password,
            const std::string& clientId,
            const std::optional<Message>& will,
            std::function<void(Message)> receiveFunc) override;
    virtual void disconnect() override;
    virtual bool isConnected() override;

    virtual bool subscribe(const std::string& topic) override;
    virtual bool unsubscribe(const std::string& topic) override;
    virtual bool publish(const Message& message) override;
    virtual void loop() override;

private:
    FakeMqttServer& server;
    std::function<void(bool)> connectCallback;
    std::optional<size_t> connectionId;
    std::function<void(Message)> receiveFunc;
    std::vector<Message> queue;

    bool connectInner(const std::optional<Message>& will,
        std::function<void(Message)> receiveFunc);
};


#endif // TEST_FAKEMQTTCONNECTION_HPP
