#ifndef CONTENT_HPP
#define CONTENT_HPP

#include <Arduino.h>

#include <vector>

String getContent(const String& method, const String& path,
        const String& content);

#endif // CONTENT_HPP
