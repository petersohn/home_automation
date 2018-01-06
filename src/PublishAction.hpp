#ifndef PUBLISHACTION_HPP
#define PUBLISHACTION_HPP

#include "common/Action.hpp"

class PublishAction : public Action {
public:
    PublishAction(const std::string& topic, const std::string& valueTemplate,
            bool retain)
            : topic(topic), valueTemplate(valueTemplate), retain(retain) {}

    void fire(const InterfaceConfig& interface);

private:
    std::string topic;
    std::string valueTemplate;
    bool retain;
};

#endif // PUBLISHACTION_HPP
