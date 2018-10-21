#ifndef OPERATION_TRANSLATOR_HPP
#define OPERATION_TRANSLATOR_HPP

#include "../tools/string.hpp"

#include <cstdlib>
#include <string>

namespace translator {

struct Str {
   const std::string& toString(const std::string& s) { return s; }
   const std::string& fromString(const std::string& s) { return s; }
};

struct Float {
   std::string toString(float i) { return tools::floatToString(i, 6); }
   float fromString(const std::string& s) {
       return std::atof(s.c_str());
   }
};

struct Bool {
   std::string toString(bool b) { return b ? "1" : "0"; }

   bool fromString(const std::string& s) {
       bool result = false;
       tools::getBoolValue(s, result);
       return result;
   }
};

} // namespace translator

#endif // OPERATION_TRANSLATOR_HPP
