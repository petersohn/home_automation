#include "DebugStream.hpp"

#include <algorithm>

int PrintStreambuf::overflow(int ch) {
    this->stream.print(traits_type::to_char_type(ch));
    return ch;
}

void DebugStreambuf::add(std::streambuf* buf) {
    this->bufs.push_back(buf);
}

void DebugStreambuf::remove(std::streambuf* buf) {
    this->bufs.erase(
        std::remove(this->bufs.begin(), this->bufs.end(), buf),
        this->bufs.end());
}

int DebugStreambuf::overflow(int ch) {
    for (auto buf : this->bufs) {
        if (buf->sputc(traits_type::to_char_type(ch)) == traits_type::eof()) {
            return traits_type::eof();
        }
    }
    return ch;
}

int DebugStreambuf::sync() {
    for (auto buf : this->bufs) {
        if (buf->pubsync() != 0) {
            return 0;
        }
    }
    return 0;
}
