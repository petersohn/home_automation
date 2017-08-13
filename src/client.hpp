#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <PubSubClient.h>

namespace mqtt {

extern PubSubClient client;

bool loop();

} // namespace mqtt

#endif // CLIENT_HPP
