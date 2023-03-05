#include "MqttConnectionImpl.hpp"

static_assert(MQTT_MAX_PACKET_SIZE == 256, "check MQTT packet size");

bool MqttConnectionImpl::connect(const char* host, uint16_t port,
        const char* username, const char* password,
        const char* clientId,
        const std::optional<Message>& will,
        ReceiveHandler receiveFunc)
{
    receiveHandler = std::move(receiveFunc);
    mqttClient.setServer(host, port).setClient(wifiClient).setCallback(
            [this](const char* topic, const unsigned char* payload,
                    unsigned length) {
                onMessageReceived(topic, payload, length);
            });
    if (will) {
        return mqttClient.connect(
                clientId, username, password,
                will->topic, 0, will->retain, will->payload);
    } else {
        return mqttClient.connect(clientId, username, password);
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

bool MqttConnectionImpl::subscribe(const char* topic)
{
    return mqttClient.subscribe(topic);
}

bool MqttConnectionImpl::unsubscribe(const char* topic)
{
    return mqttClient.unsubscribe(topic);
}

bool MqttConnectionImpl::publish(const Message& message)
{
    return mqttClient.publish(
            message.topic, message.payload, message.retain);
}

void MqttConnectionImpl::loop()
{
    mqttClient.loop();
}

void MqttConnectionImpl::onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length) {
    if (receiveHandler) {
        lock.lock();
        Message msg{
                topic,
                reinterpret_cast<const char*>(payload),
                length,
                false};
        receiveHandler(msg);
        lock.unlock();
    }
}

