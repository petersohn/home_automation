#include "EspTestBase.hpp"

TEST_F(EspTestBase, ExpectedLogFound) {
    auto e = this->expectLog("foo");
    this->debug << "bar" << std::endl;
    this->debug << "foobar" << std::endl;
    this->debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
}

TEST_F(EspTestBase, ExpectedNoLog) {
    auto e = this->expectLog("baz", 0);
    this->debug << "bar" << std::endl;
    this->debug << "foobar" << std::endl;
    this->debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
}

TEST_F(EspTestBase, ExpectedMultipleLogs) {
    auto e = this->expectLog("foo", 3);
    this->debug << "foofoo" << std::endl;
    this->debug << "bar" << std::endl;
    this->debug << "foobar" << std::endl;
    this->debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
    this->debug << "barfoo" << std::endl;
}
