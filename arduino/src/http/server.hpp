#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include "http.hpp"

#include <Arduino.h>

namespace http {

#define CHECKED_RECEIVE(function, stream, headers, sendBody, ...) \
    if (!function(stream, __VA_ARGS__)) { \
        sendError(stream, 400, "Bad Request", headers, sendBody); \
        return; \
    }

String getContent(const String& path);

template <typename Stream>
void serve(Stream& stream) {
    static const Header staticHeaders[] = {
            {"Accept", "GET, HEAD"},
            {"Connection", "close"},
            {nullptr, nullptr}
    };

    String method;
    String path;
    CHECKED_RECEIVE(receiveRequest, stream, staticHeaders, method != "HEAD",
            method, path);
    bool isGet = (method == "GET");
    bool isHead = (method == "HEAD");


    String headerName;
    String headerValue;
    while (true) {
        CHECKED_RECEIVE(receiveHeader, stream, staticHeaders, !isHead,
                headerName, headerValue);
        if (headerName.length() == 0) {
            break;
        }
        // Currently ignore all headers
    }

    if (isGet || isHead) {
        String content = getContent(path);
        if (content.length() == 0) {
            sendError(stream, 404, "Not Found", staticHeaders, !isHead);
        } else {
            sendResponse(stream, 200, "OK");
        }
        sendHeader(stream, "Content-Length", content.length());
        sendHeaders(stream, staticHeaders);
        sendHeadersEnd(stream);

        if (isGet) {
            stream.print(content);
        }
    }
}

#undef CHECKED_RECEIVE

} // namespace http


#endif // HTTP_SERVER_HPP
