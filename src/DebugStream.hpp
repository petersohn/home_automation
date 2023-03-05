#ifndef DEBUGSTREAM_HPP
#define DEBUGSTREAM_HPP

#include <Print.h>

#include <streambuf>
#include <vector>

class PrintStreambuf: public std::streambuf {
public:
    PrintStreambuf(Print& stream): stream(stream) {}
protected:
    virtual int overflow(int ch) override;
private:
    Print& stream;
};

class DebugStreambuf: public std::streambuf {
public:
    void add(std::streambuf* buf);
    void remove(std::streambuf* buf);
private:
    std::vector<std::streambuf*> bufs;

    virtual int overflow(int ch) override;
    virtual int sync() override;
};

#endif // DEBUGSTREAM_HPP
