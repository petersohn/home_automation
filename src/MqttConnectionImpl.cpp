#include "MqttConnectionImpl.hpp"

static_assert(MQTT_MAX_PACKET_SIZE == 256, "check MQTT packet size");

bool MqttConnectionImpl::connect(
    const char* host, uint16_t port, const char* username, const char* password,
    const char* clientId, const std::optional<Message>& will,
    ReceiveHandler receiveFunc) {
    this->receiveHandler = std::move(receiveFunc);
    this->mqttClient.setServer(host, port)
        .setClient(this->wifiClient)
        .setCallback([this](
                         const char* topic, const unsigned char* payload,
                         unsigned length) {
        this->onMessageReceived(topic, payload, length);
    });
    if (will) {
        return this->mqttClient.connect(
            clientId, username, password, will->topic, 0, will->retain,
            will->payload);
    } else {
        return this->mqttClient.connect(clientId, username, password);
    }
}

void MqttConnectionImpl::disconnect() {
    this->mqttClient.disconnect();
}

bool MqttConnectionImpl::isConnected() {
    return this->mqttClient.connected();
}

bool MqttConnectionImpl::subscribe(const char* topic) {
    return this->mqttClient.subscribe(topic);
}

bool MqttConnectionImpl::unsubscribe(const char* topic) {
    return this->mqttClient.unsubscribe(topic);
}

bool MqttConnectionImpl::publish(const Message& message) {
    return this->mqttClient.publish(
        message.topic, message.payload, message.retain);
}

void MqttConnectionImpl::loop() {
    this->mqttClient.loop();
}

void MqttConnectionImpl::onMessageReceived(
    const char* topic, const unsigned char* payload, unsigned length) {
    if (this->receiveHandler) {
        this->lock.lock();
        Message msg{
            topic, reinterpret_cast<const char*>(payload), length, false};
        this->receiveHandler(msg);
        this->lock.unlock();
    }
}
