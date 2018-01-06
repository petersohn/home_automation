#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <PubSubClient.h>

#include <functional>
#include <string>

namespace mqtt {

extern PubSubClient client;

bool loop();
void subscribe(const std::string& topic,
        std::function<void(const std::string&)> callback);
void unsubscribe(const std::string& topic);

} // namespace mqtt

#endif // CLIENT_HPP
