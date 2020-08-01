#ifndef PUBLISHACTION_HPP
#define PUBLISHACTION_HPP

#include "common/Action.hpp"
#include "operation/Operation.hpp"

class PublishAction : public Action {
public:
    PublishAction(const std::string& topic,
            std::unique_ptr<operation::Operation>&& operation,
            bool retain, unsigned minimumSendInterval)
            : topic(topic), operation(std::move(operation)), retain(retain),
              minimumSendInterval(minimumSendInterval), lastSend(0) {}

    void fire(const InterfaceConfig& interface);

private:
    std::string topic;
    std::unique_ptr<operation::Operation> operation;
    bool retain;
    const unsigned minimumSendInterval;
    unsigned lastSend;
};

#endif // PUBLISHACTION_HPP
