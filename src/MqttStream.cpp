#include "MqttStream.hpp"

int MqttStreambuf::overflow(int ch) {
    if (this->lock.isFree()) {
        this->msg[this->length++] = static_cast<char>(ch);
        if (this->length == maxLength) {
            pubsync();
        }
    }
    return ch;
}

int MqttStreambuf::sync() {
    if (this->length == 0) {
        return 0;
    }

    if (this->lock.isFree()) {
        this->lock.lock();
        if (this->msg[this->length - 1] == '\n') {
            --this->length;
        }
        this->msg[this->length] = '\0';
        this->mqttClient.publish(this->topic.c_str(), this->msg, false);
        this->lock.unlock();
    }

    this->length = 0;
    return 0;
}
