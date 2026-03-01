#ifndef STRINGSTREAM_HPP
#define STRINGSTREAM_HPP

#include <Print.h>

#include <string>

class StringStream : public Print {
public:
    virtual std::size_t write(uint8_t c) override;
    virtual std::size_t write(const uint8_t* value, std::size_t s) override;

    const std::string& get() const { return this->value; }
    void clear() { this->value.clear(); }

private:
    std::string value;
};

#endif  // STRINGSTREAM_HPP
