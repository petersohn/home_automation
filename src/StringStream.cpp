#include "StringStream.hpp"

std::size_t StringStream::write(uint8_t c) {
    value += static_cast<char>(c);
    return 1;
}

std::size_t StringStream::write(const uint8_t* s, std::size_t length) {
    value += std::string(reinterpret_cast<const char*>(s), length);
    return length;
}
