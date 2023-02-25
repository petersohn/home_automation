#ifndef PUBLISHACTION_HPP
#define PUBLISHACTION_HPP

#include "common/Action.hpp"
#include "common/EspApi.hpp"
#include "operation/Operation.hpp"
#include "common/MqttClient.hpp"

#include <ostream>

class PublishAction : public Action {
public:
    PublishAction(std::ostream& debug, EspApi& esp, MqttClient& mqttClient,
            const std::string& topic,
        std::unique_ptr<operation::Operation>&& operation,
        bool retain, unsigned minimumSendInterval);

    void fire(const InterfaceConfig& interface);

private:
    std::ostream& debug;
    EspApi& esp;
    MqttClient& mqttClient;

    std::string topic;
    std::unique_ptr<operation::Operation> operation;
    bool retain;
    const unsigned minimumSendInterval;
    unsigned lastSend;
};

#endif // PUBLISHACTION_HPP
