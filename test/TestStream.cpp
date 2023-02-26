#include "TestStream.hpp"

#include <boost/test/unit_test.hpp>

int TestStreambuf::sync() {
    auto s = this->str();
    if (s.back() == '\n') {
        s.resize(s.size() - 1);
    }
    if (esp) {
        BOOST_TEST_MESSAGE(esp->millis() << " " << s);
    } else {
        BOOST_TEST_MESSAGE(s);
    }
    this->str("");
    return 0;
}
