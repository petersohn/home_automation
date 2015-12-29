#ifndef TEST_CONTENT_HPP
#define TEST_CONTENT_HPP

#include <Arduino.h>

String getFullStatus(const String& type);

String getContent(const String& path, const String& content);

String getModifiedPinsContent(const String& type);

#endif // TEST_CONTENT_HPP
