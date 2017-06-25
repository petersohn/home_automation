#ifndef PUBLISHACTION_HPP
#define PUBLISHACTION_HPP

#include <Arduino.h>

#include "Action.hpp"

class PublishAction : public Action {
public:
    PublishAction(const String& topic, bool retain)
            : topic(topic), retain(retain) {}

    void fire(const String& value);

private:
    String topic;
    bool retain;
};

#endif // PUBLISHACTION_HPP
