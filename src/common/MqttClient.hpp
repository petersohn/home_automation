#ifndef COMMON_MQTTCLIENT_HPP
#define COMMON_MQTTCLIENT_HPP

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "ArduinoJson.hpp"
#include "Backoff.hpp"
#include "CycleTimer.hpp"
#include "EspApi.hpp"
#include "GlobalConfig.hpp"
#include "MqttConnection.hpp"
#include "MqttTransportState.hpp"
#include "OtaTypes.hpp"
#include "Wifi.hpp"

class Client;

struct MqttConfig {
    std::string name;
    std::vector<ServerConfig> servers;
    TopicConfig topics;
};

enum class MqttReadiness { checking, ready, collision };

class MqttClient {
public:
    MqttClient(
        std::ostream& debug, EspApi& esp, Wifi& wifi, Backoff& backoff,
        MqttConnection& connection, CycleTimer& cycleTimer,
        std::function<void()> onConnected);

    void setConfig(MqttConfig config_);
    void loop();
    void disconnect();
    bool isConnected() const;
    MqttTransportState getTransportState();
    MqttReadiness getReadiness() const;
    bool takeCollision();
    void setOtaStatusProvider(std::function<OtaStatusSnapshot()> provider);
    bool subscriptionReady(const char* topic) const;

    void subscribe(
        const char* topic,
        std::function<void(const MqttConnection::Message&)> callback,
        uint8_t qos = 0);
    void subscribePriority(
        const char* topic,
        std::function<void(const MqttConnection::Message&)> callback,
        uint8_t qos = 0);
    void unsubscribe(const char* topic);
    void publish(const char* topic, const char* payload, bool retain);
    bool canPublish() const;
    bool tryPublish(const char* topic, const char* payload, bool retain);

    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

private:
    enum class ConnectStatus {
        connecting,
        connectionSuccessful,
        connectionFailed
    };

    struct Subscription {
        std::string topic;
        std::function<void(const MqttConnection::Message&)> callback;
        uint8_t qos;
        bool priority = false;
        bool successful = false;
        uint32_t successGeneration = 0;
        unsigned long retryAt = 0;
    };

    std::ostream& debug;
    EspApi& esp;
    Wifi& wifi;
    Backoff& backoff;
    MqttConnection& connection;
    CycleTimer& cycleTimer;
    std::function<void()> onConnected;

    MqttConfig config;

    enum class InitState {
        Begin,
        ReceivedAvailable,
        ReceivedOtherDevice,
        Done,
    } initState;

    unsigned long nextConnectionAttempt = 0;
    unsigned long availabilityReceiveTimeLimit = 0;
    unsigned currentBackoff;
    unsigned long nextStatusSend = 0;

    bool initialized = false;
    bool restarted = true;
    MqttTransportState transportState{false, 0};
    MqttReadiness readiness = MqttReadiness::checking;
    bool collisionLatched = false;

    static constexpr size_t mqttPacketSize = 1024;
    static constexpr size_t mqttPacketHeaderSize = 7;
    static constexpr size_t statusMsgBufSize = 1536;
    static constexpr size_t statusMsgSize = mqttPacketSize;

    ArduinoJson::StaticJsonBuffer<statusMsgBufSize> statusMsgBuf;
    char statusMsg[statusMsgSize + 1];

    std::vector<Subscription> subscriptions;
    std::function<OtaStatusSnapshot()> otaStatusProvider;

    const char* getStatusMessage(bool restarted);
    void availabiltyReceiveSuccess();
    const char* currentStateDebug() const;
    void availabiltyReceiveFail();
    void connectionBackoff();
    void resetConnectionBackoff();
    void handleAvailabilityMessage(bool available);
    void refreshAvailability();
    void handleStatusMessage(const ArduinoJson::JsonObject& message);
    void handleMessage(const MqttConnection::Message& message);
    void updateTransportState();
    void disconnectTransport();
    bool tryToConnect(const ServerConfig& server);
    ConnectStatus connectIfNeeded();
    void sendStatusMessage(bool restarted);
    void connectedLoop();
    void subscribeWithPriority(
        const char* topic,
        std::function<void(const MqttConnection::Message&)> callback,
        uint8_t qos, bool priority);
    void attemptSubscription(Subscription& subscription);
    void retrySubscriptions();
};

#endif  // COMMON_MQTTCLIENT_HPP
