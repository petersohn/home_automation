#ifndef WIFI_HPP
#define WIFI_HPP

#include <Arduino.h>

namespace wifi {

bool connectIfNeeded(const String& ssid, const String& password);

} // namespace wifi

#endif // WIFI_HPP
