#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <PubSubClient.h>

namespace mqtt {

extern PubSubClient client;

bool connectIfNeeded();

} // namespace mqtt

#endif // CLIENT_HPP
