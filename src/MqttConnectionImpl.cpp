#include "MqttConnectionImpl.hpp"

static_assert(MQTT_MAX_PACKET_SIZE == 256, "check MQTT packet size");

bool MqttConnectionImpl::connect(const std::string& host, uint16_t port,
        const std::string& username, const std::string& password,
        const std::string& clientId,
        const std::optional<Message>& will,
        std::function<void(Message)> receiveFunc)
{
    receiveHandler = std::move(receiveFunc);
    mqttClient.setServer(host.c_str(), port).setClient(wifiClient).setCallback(
            [this](const char* topic, const unsigned char* payload,
                    unsigned length) {
                onMessageReceived(topic, payload, length);
            });
    if (will) {
        return mqttClient.connect(
                clientId.c_str(), username.c_str(), password.c_str(),
                will->topic.c_str(), 0, will->retain, will->payload.c_str());
    } else {
        return mqttClient.connect(
                clientId.c_str(), username.c_str(), password.c_str());
    }
}

void MqttConnectionImpl::disconnect()
{
    mqttClient.disconnect();
}

bool MqttConnectionImpl::isConnected()
{
    return mqttClient.connected();
}

bool MqttConnectionImpl::subscribe(const std::string& topic)
{
    return mqttClient.subscribe(topic.c_str());
}

bool MqttConnectionImpl::unsubscribe(const std::string& topic)
{
    return mqttClient.unsubscribe(topic.c_str());
}

bool MqttConnectionImpl::publish(const Message& message)
{
    return mqttClient.publish(
    		message.topic.c_str(), message.payload.c_str(), message.retain);
}

void MqttConnectionImpl::loop()
{
    mqttClient.loop();
}

void MqttConnectionImpl::onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length) {
    if (receiveHandler) {
        receiveHandler(Message{topic,
                std::string{reinterpret_cast<const char*>(payload), length},
                false});
    }
}

