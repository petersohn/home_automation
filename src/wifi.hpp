#ifndef WIFI_HPP
#define WIFI_HPP

#include <Arduino.h>

namespace wifi {

void init();
bool connectIfNeeded(const std::string& ssid, const std::string& password);

} // namespace wifi

#endif // WIFI_HPP
