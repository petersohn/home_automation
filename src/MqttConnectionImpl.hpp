#ifndef MQTTCONNECTIONIMPL_HPP
#define MQTTCONNECTIONIMPL_HPP

#include "common/MqttConnection.hpp"

#include <PubSubClient.h>
#include <ESP8266WiFi.h>


class MqttConnectionImpl : public MqttConnection {
public:
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
    WiFiClient wifiClient;
    PubSubClient mqttClient;

    std::function<void(Message)> receiveHandler;

    void onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length);
};


#endif // MQTTCONNECTIONIMPL_HPP
