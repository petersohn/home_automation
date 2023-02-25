#ifndef COMMON_MQTTCLIENT_HPP
#define COMMON_MQTTCLIENT_HPP

#include "EspApi.hpp"
#include "Backoff.hpp"
#include "Wifi.hpp"
#include "MqttConnection.hpp"

#include <functional>
#include <string>
#include <ostream>
#include <memory>
#include <vector>

class Client;
struct ServerConfig;

namespace ArduinoJson {
class JsonObject;
}

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
    MqttClient(std::ostream& debug, EspApi& esp, Wifi& wifi, Backoff& backoff,
            MqttConnection& connection, std::function<void()> onConnected);

    void setConfig(MqttConfig config_);
    void loop();
    void disconnect();
    bool isConnected() const;

    void subscribe(const std::string& topic,
            std::function<void(const std::string&)> callback);
    void unsubscribe(const std::string& topic);
    void publish(const std::string& topic, const std::string& payload, bool retain);

    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

private:
    enum class ConnectStatus {
        connecting,
        connectionSuccessful,
        connectionFailed
    };

    using Subscription = std::pair<std::string,
          std::function<void(const std::string&)>>;

    std::ostream& debug;
    EspApi& esp;
    Wifi& wifi;
    Backoff& backoff;
    MqttConnection& connection;
    std::function<void()> onConnected;

    MqttConfig config;

    unsigned long nextConnectionAttempt = 0;
    unsigned long availabilityReceiveTimeLimit = 0;
    unsigned currentBackoff;
    unsigned long nextStatusSend = 0;
    bool initialized = false;
    std::string willMessage;
    bool restarted = true;

    std::vector<MqttConnection::Message> receivedMessages;
    std::vector<Subscription> subscriptions;

    std::string getStatusMessage(bool available, bool restarted);
    void resetAvailabilityReceive();
    void connectionBackoff();
    void resetConnectionBackoff();
    void handleAvailabilityMessage(const ArduinoJson::JsonObject& message);
    void onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length);
    void handleMessage(const MqttConnection::Message& message);
    bool tryToConnect(const ServerConfig& server);
    ConnectStatus connectIfNeeded();
    void sendStatusMessage(bool restarted);
};

#endif // COMMON_MQTTCLIENT_HPP
