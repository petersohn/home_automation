#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <PubSubClient.h>

#include <functional>

namespace mqtt {

extern PubSubClient client;

bool loop();
void subscribe(const String& topic,
        std::function<void(const String&)> callback);
void unsubscribe(const String& topic);

} // namespace mqtt

#endif // CLIENT_HPP
