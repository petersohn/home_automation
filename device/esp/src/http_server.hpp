#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include "http.hpp"
#include "string.hpp"

#include <Arduino.h>

namespace http {

namespace detail {

template <typename Stream>
class Request {
public:
    Request(Stream& stream) : stream(stream) {}

    template <typename ContentProvider>
    void serve(const ContentProvider& contentProvider) {
        if (!receiveRequest(stream, method, path)) {
            isHead = (method == "HEAD");
            sendError(400, "Bad Request", "Invalid request header");
            return;
        }
        isGet = (method == "GET");
        isHead = (method == "HEAD");

        String incomingContent;
        if (!readHeadersAndContent(stream, incomingContent, connection)) {
            connection = "close";
            sendError(400, "Bad Request", "Invalid format");
            return;
        }

        if (isGet || isHead) {
            String content = contentProvider(path, incomingContent);
            if (content.length() == 0) {
                sendError(404, "Not Found", "Invalid path: " + path);
            } else {
                sendAnswer(200, "OK", content);
            }
        } else {
            sendError(405, "Method Not Allowed", "Invalid method: " +
                    method);
        }
    }

    bool isOk() {
        return statusCode >= 200 && statusCode < 300;
    }

private:
    void sendError(int statusCode, const char* description,
            const String& details) {
        sendAnswer(statusCode, description, createErrorContent(
                statusCode, description, details));
    }

    void sendAnswer(int statusCode, const char* description,
            const String& content) {
        sendResponse(stream, statusCode, description);
        sendHeader(stream, "Accept", "GET, HEAD");
        sendHeader(stream, "Content-Length", content.length());
        sendHeader(stream, "Content-Type", "application/json");
        sendHeader(stream, "Connection", connection);
        sendHeadersEnd(stream);

        if (!isHead) {
            stream.print(content);
        }
        if (connection != "keep-alive") {
            stream.stop();
        }
    }

    Stream& stream;
    String method;
    String path;
    String connection = "close";
    bool isGet = false;
    bool isHead = false;
    int statusCode = 0;
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
