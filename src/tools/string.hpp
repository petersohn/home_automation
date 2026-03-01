#ifndef TOOLS_STRING_HPP
#define TOOLS_STRING_HPP

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace tools {

std::string nextToken(
    const std::string& string, char separator, size_t& position);

std::string intToString(int i, unsigned radix = 10);
std::string floatToString(double i, int decimals);

class Join {
public:
    Join(const char* separator) : separator(separator) {}

    void add(const std::string& value) {
        if (this->first) {
            this->first = false;
        } else {
            this->result += this->separator;
        }
        this->result += value;
    }

    const std::string& get() const { return this->result; }

private:
    const char* separator;
    std::string result;
    bool first = true;
};

namespace detail {

template <typename Range>
void addValue(
    std::string& result, const std::string& reference, const Range& elements) {
    std::size_t value = std::atol(reference.c_str());
    if (value > 0 && value <= elements.size()) {
        result += elements[value - 1];
    }
}

}  // namespace detail

template <typename Range>
std::string substitute(
    const std::string& valueTemplate, const Range& elements) {
    std::string result;
    std::string reference;
    bool inReference = false;
    for (std::size_t i = 0; i < valueTemplate.length(); ++i) {
        if (inReference) {
            if (valueTemplate[i] >= '0' && valueTemplate[i] <= '9') {
                reference += valueTemplate[i];
            } else {
                inReference = false;
                if (reference.length() == 0) {
                    result += valueTemplate[i];
                    reference = "";
                    continue;
                }
                detail::addValue(result, reference, elements);
                reference = "";
            }
        }
        if (!inReference) {
            if (valueTemplate[i] == '%') {
                inReference = true;
            } else {
                result += valueTemplate[i];
            }
        }
    }
    detail::addValue(result, reference, elements);
    return result;
}

bool getBoolValue(const char* input, bool& output, int length = -1);

}  // namespace tools

#endif  // TOOLS_STRING_HPP
