#include <algorithm>

#include "DebugStream.hpp"

int PrintStreambuf::overflow(int ch) {
    stream.print(traits_type::to_char_type(ch));
    return ch;
}

void DebugStreambuf::add(std::streambuf* buf) {
    bufs.push_back(buf);
}

void DebugStreambuf::remove(std::streambuf* buf) {
    bufs.erase(std::remove(bufs.begin(), bufs.end(), buf), bufs.end());
}

int DebugStreambuf::overflow(int ch) {
    for (auto buf : bufs) {
        if (buf->sputc(traits_type::to_char_type(ch)) == traits_type::eof()) {
            return traits_type::eof();
        }
    }
    return ch;
}

int DebugStreambuf::sync() {
    for (auto buf : bufs) {
        if (buf->pubsync() != 0) {
            return 0;
        }
    }
    return 0;
}
