#ifndef HOME_ASSISTANT_HPP
#define HOME_ASSISTANT_HPP

#include "http_client.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>

template<typename Connection>
bool sendHomeAssistantUpdate(Connection& connection,
        InterfaceConfig& interface, bool allowCached) {
    if (!http::connectIfNeeded(connection, globalConfig.serverAddress,
            globalConfig.serverPort)) {
        return false;
    }
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
    bool success = sendRequest(connection, requestInfo, response);
    DEBUGLN(response);
    return success;
}

#endif // HOME_ASSISTANT_HPP
