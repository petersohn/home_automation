#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include "http.hpp"
#include "string.hpp"

#include <Arduino.h>

#include <algorithm>

namespace http {

namespace detail {

struct StatusCode {
    int code;
    const char* description;
};

template <typename Stream>
class Request {
public:
    Request(Stream& stream) : stream(stream) {}

    template <typename ContentProvider>
    void serve(const ContentProvider& contentProvider) {
    DynamicJsonBuffer buffer{512};
        if (!receiveRequest(stream, method, path)) {
            isHead = (method == "HEAD");
            sendError(buffer, 400, "Invalid request header");
            return;
        }
        isHead = (method == "HEAD");
        if (isHead) {
            method = "GET";
        }

        String incomingContent;
        if (!readHeadersAndContent(stream, incomingContent, connection)) {
            connection = "close";
            sendError(buffer, 400, "Invalid format");
            return;
        }

        if (method == "GET" || method == "POST") {
            HttpResult result = contentProvider(
                    buffer, method, path, incomingContent);
            sendAnswer(result.statusCode, getDescription(statusCode),
                    result.value.as<JsonObject>());
        } else {
            sendError(buffer, 405, "Invalid method: " + method);
        }
    }

    bool isOk() {
        return statusCode >= 200 && statusCode < 300;
    }

private:
    static const std::initializer_list<StatusCode> statusCodes;

    JsonObject& createErrorContent(JsonBuffer& buffer, int statusCode,
            const char* description, const String& details) {
        JsonObject& result = buffer.createObject();

        result["statusCode"] = statusCode;
        result["description"] = description;
        result["details"] = details;

        return result;
    }

    const char* getDescription(int statusCode) {
        auto iterator = std::find_if(statusCodes.begin(), statusCodes.end(),
                [statusCode](const StatusCode& code) {
                    return code.code == statusCode;
                });
        return (iterator == statusCodes.end())
                ? "" : iterator->description;
    }


    void sendError(JsonBuffer& buffer, int statusCode, const String& details) {
        const char* description = getDescription(statusCode);
        sendAnswer(statusCode, description, createErrorContent(buffer,
                statusCode, description, details));
    }

    void sendAnswer(int statusCode, const char* description,
            JsonVariant content) {
        String contentString;
        if (content.is<JsonObject>()) {
            content.as<JsonObject>().printTo(contentString);
        } else if (content.is<JsonArray>()) {
            content.as<JsonArray>().printTo(contentString);
        } else {
            contentString = content.as<const char*>();
        }
        sendResponse(stream, statusCode, description);
        sendHeader(stream, "Accept", "GET, HEAD");
        sendHeader(stream, "Content-Length", String(contentString.length()));
        sendHeader(stream, "Content-Type", "application/json");
        sendHeader(stream, "Connection", connection);
        sendHeadersEnd(stream);

        if (!isHead) {
            stream.print(contentString);
        }
        if (connection != "keep-alive") {
            stream.stop();
        }
    }

    Stream& stream;
    String method;
    String path;
    String connection = "close";
    bool isHead = false;
    int statusCode = 0;
};

template <typename Stream>
const std::initializer_list<StatusCode> Request<Stream>::statusCodes = {
    StatusCode{200, "OK"},
    StatusCode{400, "Bad Request"},
    StatusCode{404, "Not Found"},
    StatusCode{405, "Method Not Allowed"},
};


} // namespace detail

template <typename Stream, typename ContentProvider>
bool serve(Stream& stream, ContentProvider contentProvider) {
    detail::Request<Stream> request{stream};
    request.serve(contentProvider);
    return request.isOk();
}

#undef CHECKED_RECEIVE

} // namespace http


#endif // HTTP_SERVER_HPP
