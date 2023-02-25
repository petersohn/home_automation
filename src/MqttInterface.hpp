#ifndef MQTTINTERFACE_HPP
#define MQTTINTERFACE_HPP

#include "common/Interface.hpp"
#include "common/MqttClient.hpp"

class MqttInterface : public Interface {
public:
    MqttInterface(MqttClient& mqttClient, const std::string& topic)
            : mqttClient(mqttClient), topic(topic) {}
    ~MqttInterface();

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    MqttClient& mqttClient;

    std::string topic;
    std::vector<std::string> messages;

    void onMessage(const std::string& message);
};

#endif // MQTTINTERFACE_HPP
