#ifndef MQTTSTREAM_HPP
#define MQTTSTREAM_HPP

#include "common/MqttClient.hpp"

#include <sstream>

class MqttStreambuf: public std::stringbuf {
public:
	MqttStreambuf(MqttClient& mqttClient, std::string topic)
        : mqttClient(mqttClient), topic(std::move(topic)) {
    }

protected:
    virtual int overflow(int ch) override;
    virtual int sync() override;

private:
    MqttClient& mqttClient;
    std::string topic;

    bool sending = false;
};

#endif // MQTTSTREAM_HPP

