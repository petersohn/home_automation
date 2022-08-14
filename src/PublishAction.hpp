#ifndef PUBLISHACTION_HPP
#define PUBLISHACTION_HPP

#include "common/Action.hpp"
#include "operation/Operation.hpp"
#include "client.hpp"

#include <ostream>

class PublishAction : public Action {
public:
    PublishAction(std::ostream& debug, MqttClient& mqttClient,
            const std::string& topic,
        std::unique_ptr<operation::Operation>&& operation,
        bool retain, unsigned minimumSendInterval);

    void fire(const InterfaceConfig& interface);

private:
    std::ostream& debug;
    MqttClient& mqttClient;

    std::string topic;
    std::unique_ptr<operation::Operation> operation;
    bool retain;
    const unsigned minimumSendInterval;
    unsigned lastSend;
};

#endif // PUBLISHACTION_HPP
