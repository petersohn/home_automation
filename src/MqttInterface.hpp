#ifndef MQTTINTERFACE_HPP
#define MQTTINTERFACE_HPP

#include "Interface.hpp"

class MqttInterface : public Interface {
public:
    MqttInterface(const String& topic)
            : topic(topic) {}
    ~MqttInterface();

    void start() override;
    void execute(const String& command) override;
    void update(Actions action) override;

private:
    void onMessage(const String& message);

    String topic;
    std::vector<String> messages;
};

#endif // MQTTINTERFACE_HPP
