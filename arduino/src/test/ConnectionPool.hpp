#ifndef TEST_CONNECTIONPOOL_HPP
#define TEST_CONNECTIONPOOL_HPP

#include <Arduino.h>

template <typename Connection>
class ConnectionPool {
public:
    void add(const Connection& connection) {
        unsigned long leastTime = time;
        PoolElement* element = nullptr;
        for (size_t i = 0; i < poolSize; ++i) {
            if (!connections[i].connection.connected()) {
                element = &connections[i];
                break;
            }
            if (leastTime > connections[i].lastSeen) {
                leastTime = connections[i].lastSeen;
                element = &connections[i];
            }
        }
        Serial.println(element - connections);
        element->connection.stop();
        element->connection = connection;
        element->lastSeen = time++;
        Serial.println(element->lastSeen);
    }

    template <typename ServeFunction>
    void serve(const ServeFunction& serveFunction) {
        for (size_t i = 0; i < poolSize; ++i) {
            if (connections[i].lastSeen != 0 &&
                    !connections[i].connection.connected()) {
                connections[i].connection.stop();
                connections[i].lastSeen = 0;
                continue;
            }
            if (connections[i].connection.available()) {
                Serial.print("Incoming request from ");
                Serial.println(connections[i].connection.remoteIP());
                connections[i].lastSeen = time++;
                serveFunction(connections[i].connection);
            }
        }
    }
private:
    struct PoolElement {
        Connection connection;
        unsigned long lastSeen;
    };
    static constexpr size_t poolSize = 4;
    PoolElement connections[poolSize];
    unsigned long time = 0;
};


#endif // TEST_CONNECTIONPOOL_HPP
