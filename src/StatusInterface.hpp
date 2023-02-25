#ifndef STATUSINTERFACE_HPP
#define STATUSINTERFACE_HPP

#include "common/Interface.hpp"
#include "common/MqttClient.hpp"

class StatusInterface : public Interface {
public:
    StatusInterface(MqttClient& mqttClient): mqttClient(mqttClient) {}

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    MqttClient& mqttClient;

    int value = -1;
};

#endif // STATUSINTERFACE_HPP

