#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <PubSubClient.h>

#include <functional>
#include <string>

namespace mqtt {

void loop();
void disconnect();

void subscribe(const std::string& topic,
        std::function<void(const std::string&)> callback);
void unsubscribe(const std::string& topic);
void publish(const std::string& topic, const std::string& payload, bool retain);

} // namespace mqtt

#endif // CLIENT_HPP
