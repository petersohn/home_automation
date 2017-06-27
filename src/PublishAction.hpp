#ifndef PUBLISHACTION_HPP
#define PUBLISHACTION_HPP

#include "Action.hpp"

#include <Arduino.h>

class PublishAction : public Action {
public:
    PublishAction(const String& topic, const String& valueTemplate, bool retain)
            : topic(topic), valueTemplate(valueTemplate), retain(retain) {}

    void fire(const std::vector<String>& values);

private:
    String topic;
    String valueTemplate;
    bool retain;
};

#endif // PUBLISHACTION_HPP
