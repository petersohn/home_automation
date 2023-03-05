#ifndef MQTTCONNECTIONIMPL_HPP
#define MQTTCONNECTIONIMPL_HPP

#include "common/MqttConnection.hpp"
#include "common/Lock.hpp"

#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#include <ostream>


class MqttConnectionImpl : public MqttConnection {
public:
    MqttConnectionImpl(std::ostream& debug, Lock& lock)
        : debug(debug), lock(lock) {}

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
    std::ostream& debug;
    Lock& lock;
    WiFiClient wifiClient;
    PubSubClient mqttClient;

    ReceiveHandler receiveHandler;

    void onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length);
};


#endif // MQTTCONNECTIONIMPL_HPP
