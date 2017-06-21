#ifndef CONTENT_HPP
#define CONTENT_HPP

#include "result.hpp"

#include <Arduino.h>

#include <vector>

HttpResult getContent(DynamicJsonBuffer& buffer, const String& method,
        const String& path, const String& content);

#endif // CONTENT_HPP
