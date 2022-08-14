#ifndef DEBUGSTREAM_HPP
#define DEBUGSTREAM_HPP

#include <Print.h>

#include <streambuf>

class PrintStreambuf: public std::streambuf {
public:
    PrintStreambuf(Print& stream): stream(stream) {}
protected:
    virtual int overflow(int ch) override;
private:
    Print& stream;
};

//class MqttStreambuf: public std::streambuf {
//public:
//    MqttStreambuf(Print& stream): stream(stream) {}
//protected:
//    virtual int overflow(int ch) override;
//    virtual int sync() override;
//private:
//    Print& stream;
//};

#endif // DEBUGSTREAM_HPP
