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
        mqttClient.publish(topic, this->str(), false);
        sending = false;
    }
    this->str("");
    return 0;
}
