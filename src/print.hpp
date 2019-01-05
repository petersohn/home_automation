#ifndef PRINT_HPP
#define PRINT_HPP

#include <Print.h>

#include <string>

inline void print(Print& stream, const std::string& s) {
    stream.print(s.c_str());
}

template<typename T>
inline void print(Print& stream, const T& t) {
    stream.print(t);
}

inline void println(Print& stream, const std::string& s) {
    stream.println(s.c_str());
}

template<typename T>
inline void println(Print& stream, const T& t) {
    stream.println(t);
}

#endif // PRINT_HPP
