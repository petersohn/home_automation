#include "DebugStream.hpp"

int PrintStreambuf::overflow(int ch) {
    stream.print(static_cast<char>(ch));
    return ch;
}
