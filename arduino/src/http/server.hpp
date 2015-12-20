#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include "http.hpp"

#include <Arduino.h>

namespace http {

namespace detail {

String getContent(const String& path);

template <typename Stream>
class Request {
public:
    Request(Stream& stream) : stream(stream) {}

    void serve() {
        if (!receiveRequest(stream, method, path)) {
            isHead = (method == "HEAD");
            sendError(400, "Bad Request");
            return;
        }
        isGet = (method == "GET");
        isHead = (method == "HEAD");

        String headerName;
        String headerValue;
        while (true) {
            if (!receiveHeader(stream, headerName, headerValue)) {
                sendError(400, "Bad Request");
                return;
            }
            if (headerName.length() == 0) {
                break;
            }
            //if (headerName == "Connection") {
                //connection = headerValue;
            //}
        }

        if (isGet || isHead) {
            String content = getContent(path);
            if (content.length() == 0) {
                sendError(404, "Not Found");
            } else {
                sendAnswer(200, "OK", content);
            }
        } else {
            sendError(405, "Method Not Allowed");
        }
    }
private:
    void sendError(int statusCode, const char* description) {
        sendAnswer(statusCode, description, createErrorContent(
                statusCode, description));
    }

    void sendAnswer(int statusCode, const char* description,
            const String& content) {
        sendResponse(stream, statusCode, description);
        sendHeader(stream, "Accept", "GET, HEAD");
        sendHeader(stream, "Content-Length", content.length());
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
};

} // namespace detail

template <typename Stream>
void serve(Stream& stream) {
    detail::Request<Stream>{stream}.serve();
}

#undef CHECKED_RECEIVE

} // namespace http


#endif // HTTP_SERVER_HPP
