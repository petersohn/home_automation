#ifndef COMMON_MQTTCONNECTION_HPP
#define COMMON_MQTTCONNECTION_HPP

#include <cstdint>
#include <functional>
#include <optional>

class MqttConnection {
public:
    struct Message {
        const char* topic;
        const char* payload;
        size_t payloadLength;
        bool retain;

        Message(
            const char* topic, const char* payload, size_t payloadLength,
            bool retain)
            : topic(topic)
            , payload(payload)
            , payloadLength(payloadLength)
            , retain(retain) {}
    };
    using ReceiveHandler = std::function<void(const Message&)>;

    virtual bool connect(
        const char* host, uint16_t port, const char* username,
        const char* password, const char* clientId,
        const std::optional<Message>& will, ReceiveHandler receiveFunc) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;

    virtual bool subscribe(const char* topic) = 0;
    virtual bool unsubscribe(const char* topic) = 0;
    virtual bool publish(const Message& message) = 0;
    virtual void loop() = 0;

    virtual ~MqttConnection() {}
};

#endif  // COMMON_MQTTCONNECTION_HPP
