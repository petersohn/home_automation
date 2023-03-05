#ifndef MQTTSTREAM_HPP
#define MQTTSTREAM_HPP

#include "common/MqttClient.hpp"
#include "common/Lock.hpp"

#include <string>

class MqttStreambuf: public std::streambuf {
public:
    MqttStreambuf(Lock& lock, MqttClient& mqttClient, std::string topic)
        : lock(lock), mqttClient(mqttClient), topic(std::move(topic)) {
    }

protected:
    virtual int overflow(int ch) override;
    virtual int sync() override;

private:
    static constexpr size_t maxLength = 300;

    Lock& lock;
    MqttClient& mqttClient;
    const std::string topic;
    char msg[maxLength + 1];
    size_t length = 0;

};

#endif // MQTTSTREAM_HPP

