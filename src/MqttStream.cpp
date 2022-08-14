#include "MqttStream.hpp"

int MqttStreambuf::overflow(int ch) {
    if (!sending) {
        return std::stringbuf::overflow(ch);
    }
    return ch;
}

int MqttStreambuf::sync() {
    if (!sending) {
        sending = true;
        auto msg = this->str();
        if (msg[msg.size() - 1] == '\n') {
            msg.resize(msg.size() - 1);
        }
        mqttClient.publish(topic, msg, false);
        sending = false;
    }
    this->str("");
    return 0;
}
