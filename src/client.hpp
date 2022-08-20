#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "common/EspApi.hpp"
#include "common/Wifi.hpp"

#include <PubSubClient.h>

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

class MqttClient {
public:
    MqttClient(std::ostream& debug, EspApi& esp, Wifi& wifi);

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
    struct Message {
        std::string topic;
        std::string payload;
    };

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

    unsigned long nextConnectionAttempt = 0;
    unsigned long availabilityReceiveTimeLimit = 0;
    unsigned backoff;
    unsigned long nextStatusSend = 0;
    bool initialized = false;
    std::unique_ptr<Client> wifiClient;
    std::string willMessage;
    bool restarted = true;
    std::unique_ptr<PubSubClient> client;

    std::vector<Message> receivedMessages;
    std::vector<Subscription> subscriptions;

    std::string getStatusMessage(bool available, bool restarted);
    void resetAvailabilityReceive();
    void connectionBackoff();
    void resetConnectionBackoff();
    void handleAvailabilityMessage(const ArduinoJson::JsonObject& message);
    void onMessageReceived(
        const char* topic, const unsigned char* payload, unsigned length);
    void handleMessage(const Message& message);
    bool tryToConnect(const ServerConfig& server);
    ConnectStatus connectIfNeeded();
    void sendStatusMessage(bool restarted);
};

#endif // CLIENT_HPP
