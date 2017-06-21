#ifndef RESULT_HPP
#define RESULT_HPP

#include <ArduinoJson.h>

struct HttpResult {
    int statusCode;
    JsonVariant value;
};


#endif // RESULT_HPP
