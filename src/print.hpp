#ifndef PRINT_HPP
#define PRINT_HPP

#include <Arduino.h>

#include <string>

inline void print(Stream& stream, const std::string& s) {
    stream.print(s.c_str());
}

template<typename T>
inline void print(Stream& stream, const T& t) {
    stream.print(t);
}

inline void println(Stream& stream, const std::string& s) {
    stream.println(s.c_str());
}

template<typename T>
inline void println(Stream& stream, const T& t) {
    stream.println(t);
}

#endif // PRINT_HPP
