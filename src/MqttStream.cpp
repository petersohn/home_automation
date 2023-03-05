#include "MqttStream.hpp"

int MqttStreambuf::overflow(int ch) {
    if (lock.isFree()) {
        msg[length++] = static_cast<char>(ch);
        if (length == maxLength) {
            pubsync();
        }
    }
    return ch;
}

int MqttStreambuf::sync() {
    if (length == 0) {
        return 0;
    }

    if (lock.isFree()) {
        lock.lock();
        if (msg[length - 1] == '\n') {
            --length;
        }
        msg[length] = '\0';
        mqttClient.publish(topic.c_str(), msg, false);
        lock.unlock();
    }

    length = 0;
    return 0;
}
