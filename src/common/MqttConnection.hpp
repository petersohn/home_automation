#ifndef COMMON_MQTTCONNECTION_HPP
#define COMMON_MQTTCONNECTION_HPP

#include <functional>
#include <string>
#include <optional>

class MqttConnection {
public:
    struct Message {
        std::string topic;
        std::string payload;
        bool retain;
    };

    virtual bool connect(const std::string& host, uint16_t port,
            const std::string& username, const std::string& password,
            const std::string& clientId,
            const std::optional<Message>& will,
            std::function<void(Message)> receiveFunc) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;

    virtual bool subscribe(const std::string& topic) = 0;
    virtual bool unsubscribe(const std::string& topic) = 0;
    virtual bool publish(const Message& message) = 0;
    virtual void loop() = 0;

    virtual ~MqttConnection() {}
};

#endif // COMMON_MQTTCONNECTION_HPP
