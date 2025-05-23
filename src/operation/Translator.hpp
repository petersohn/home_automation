#ifndef OPERATION_TRANSLATOR_HPP
#define OPERATION_TRANSLATOR_HPP

#include <cstdlib>
#include <string>

#include "../tools/string.hpp"

namespace translator {

struct Str {
    const std::string& toString(const std::string& s) { return s; }
    const std::string& fromString(const std::string& s) { return s; }
};

struct Float {
    std::string toString(float i) { return tools::floatToString(i, 6); }
    float fromString(const std::string& s) { return std::atof(s.c_str()); }
};

struct Bool {
    std::string toString(bool b) { return b ? "1" : "0"; }

    bool fromString(const std::string& s) {
        bool result = false;
        tools::getBoolValue(s.c_str(), result, s.size());
        return result;
    }
};

}  // namespace translator

#endif  // OPERATION_TRANSLATOR_HPP
