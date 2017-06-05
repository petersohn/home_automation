#ifndef TEST_CONNECTIONPOOL_HPP
#define TEST_CONNECTIONPOOL_HPP

#include "config/debug.hpp"

#include <Arduino.h>

#include <array>

template <typename Connection>
class ConnectionPool {
public:
    void add(const Connection& connection) {
        unsigned long leastTime = time;
        PoolElement* element = nullptr;
        for (PoolElement& connection : connections) {
            if (!connection.connection.connected()) {
                element = &connection;
                break;
            }
            if (leastTime > connection.lastSeen) {
                leastTime = connection.lastSeen;
                element = &connection;
            }
        }
        element->connection.stop();
        element->connection = connection;
        element->lastSeen = time++;
    }

    template <typename ServeFunction>
    void serve(const ServeFunction& serveFunction) {
        for (PoolElement& connection : connections) {
            if (connection.lastSeen != 0 &&
                    !connection.connection.connected()) {
                connection.connection.stop();
                connection.lastSeen = 0;
                continue;
            }
            if (connection.connection.available()) {
                DEBUG("Incoming request from ");
                DEBUGLN(connection.connection.remoteIP());
                connection.lastSeen = time++;
                serveFunction(connection.connection);
            }
        }
    }
private:
    struct PoolElement {
        Connection connection;
        unsigned long lastSeen;
    };
    std::array<PoolElement, 3> connections;
    unsigned long time = 0;
};


#endif // TEST_CONNECTIONPOOL_HPP
