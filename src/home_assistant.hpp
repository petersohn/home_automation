#ifndef HOME_ASSISTANT_HPP
#define HOME_ASSISTANT_HPP

#include "http_client.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>

template<typename Connection>
bool sendHomeAssistantUpdate(Connection& connection,
        InterfaceConfig& interface, bool allowCached) {
    StaticJsonBuffer<200> buffer;
    JsonObject& content = buffer.createObject();
    if (!allowCached || interface.lastValue.length() == 0) {
        interface.lastValue = interface.interface->get();
    }
    content["state"] = interface.lastValue;

    http::RequestInfo requestInfo;
    requestInfo.method = "POST";
    requestInfo.url = "/api/states/" + interface.name;
    requestInfo.password = globalConfig.serverPassword;
    content.printTo(requestInfo.content);

    String response;
    int statusCode = sendRequestWithRetries(connection, requestInfo, response);
    DEBUG("Result status = ");
    DEBUGLN(statusCode);
    DEBUGLN(response);
    return statusCode >= 200 && statusCode < 300;
}

#endif // HOME_ASSISTANT_HPP
