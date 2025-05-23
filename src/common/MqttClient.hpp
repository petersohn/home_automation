#ifndef COMMON_MQTTCLIENT_HPP
#define COMMON_MQTTCLIENT_HPP

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "ArduinoJson.hpp"
#include "Backoff.hpp"
#include "EspApi.hpp"
#include "MqttConnection.hpp"
#include "Wifi.hpp"

class Client;
struct ServerConfig;

struct ServerConfig {
    std::string address;
    uint16_t port = 0;
    std::string username;
    std::string password;
};

struct TopicConfig {
    std::string availabilityTopic;
    std::string statusTopic;
};

struct MqttConfig {
    std::string name;
    std::vector<ServerConfig> servers;
    TopicConfig topics;
};

class MqttClient {
public:
    MqttClient(
        std::ostream& debug, EspApi& esp, Wifi& wifi, Backoff& backoff,
        MqttConnection& connection, std::function<void()> onConnected);

    void setConfig(MqttConfig config_);
    void loop();
    void disconnect();
    bool isConnected() const;

    void subscribe(
        const char* topic,
        std::function<void(const MqttConnection::Message&)> callback);
    void unsubscribe(const char* topic);
    void publish(const char* topic, const char* payload, bool retain);

    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

private:
    enum class ConnectStatus {
        connecting,
        connectionSuccessful,
        connectionFailed
    };

    using Subscription = std::pair<
        std::string, std::function<void(const MqttConnection::Message&)>>;

    std::ostream& debug;
    EspApi& esp;
    Wifi& wifi;
    Backoff& backoff;
    MqttConnection& connection;
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

    unsigned long previousStatusSend = 0;
    unsigned long previousCycle = 0;
    unsigned long maxCycleTime = 0;
    unsigned long cycles = 0;

    static constexpr size_t statusMsgBufSize = 350;
    static constexpr size_t statusMsgSize = 250;

    ArduinoJson::StaticJsonBuffer<statusMsgBufSize> statusMsgBuf;
    char statusMsg[statusMsgSize];

    std::vector<Subscription> subscriptions;

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
    bool tryToConnect(const ServerConfig& server);
    ConnectStatus connectIfNeeded();
    void sendStatusMessage(bool restarted);
    void connectedLoop();
};

#endif  // COMMON_MQTTCLIENT_HPP
