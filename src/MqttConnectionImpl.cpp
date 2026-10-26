#include "MqttConnectionImpl.hpp"

#include <lwip/tcp.h>

#include "common/MqttDrain.hpp"

static_assert(MQTT_MAX_PACKET_SIZE == 256, "check MQTT packet size");

namespace {

// setBufferSize() controls runtime allocation; the macro sets only its default.
// Keep space for a 512-byte command plus topic and MQTT overhead.
constexpr uint16_t mqttBufferSize = 1024;

}  // unnamed namespace

void MqttConnectionImpl::TraceWiFiClient::flush() {
    const unsigned long start = millis();
    const bool result = WiFiClient::flush(0);
    Serial.printf(
        "WiFiClient.flush result=%d elapsed=%lu ms\n", result,
        millis() - start);
}

bool MqttConnectionImpl::TraceWiFiClient::drain(uint32_t timeoutMs) {
    const uint32_t startedAtMs = static_cast<uint32_t>(millis());
    bool result = false;
    while (!isDrainDeadlineExpired(
        startedAtMs, static_cast<uint32_t>(millis()), timeoutMs)) {
        if (this->status() != ESTABLISHED) {
            break;
        }
        if (isTcpDrainComplete(true, this->availableForWrite(), TCP_SND_BUF)) {
            result =
                this->status() == ESTABLISHED &&
                !isDrainDeadlineExpired(
                    startedAtMs, static_cast<uint32_t>(millis()), timeoutMs);
            break;
        }
        yield();
    }
    Serial.printf(
        "WiFiClient.drain result=%d elapsed=%lu ms\n", result,
        millis() - static_cast<unsigned long>(startedAtMs));
    return result;
}

void MqttConnectionImpl::TraceWiFiClient::stop() {
    const unsigned long start = millis();
    const bool result = WiFiClient::stop(0);
    Serial.printf(
        "WiFiClient.stop result=%d elapsed=%lu ms\n", result, millis() - start);
}

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
    // Fail connection rather than accept an undersized packet buffer.
    if (!this->mqttClient.setBufferSize(mqttBufferSize)) {
        this->debug << "Could not allocate " << mqttBufferSize
                    << "-byte MQTT buffer." << std::endl;
        return false;
    }
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

bool MqttConnectionImpl::drain(uint32_t timeoutMs) {
    return this->wifiClient.drain(timeoutMs);
}

bool MqttConnectionImpl::subscribe(const char* topic, uint8_t qos) {
    if (qos > 1) {
        return false;
    }
    return this->mqttClient.subscribe(topic, qos);
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
