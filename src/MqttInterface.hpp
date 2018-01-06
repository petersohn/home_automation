#ifndef MQTTINTERFACE_HPP
#define MQTTINTERFACE_HPP

#include "Interface.hpp"

class MqttInterface : public Interface {
public:
    MqttInterface(const std::string& topic)
            : topic(topic) {}
    ~MqttInterface();

    void start() override;
    void execute(const std::string& command) override;
    void update(Actions action) override;

private:
    void onMessage(const std::string& message);

    std::string topic;
    std::vector<std::string> messages;
};

#endif // MQTTINTERFACE_HPP
