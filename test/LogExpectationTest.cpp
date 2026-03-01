#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include "EspTestBase.hpp"

BOOST_AUTO_TEST_SUITE(LogExpectationTest)

BOOST_FIXTURE_TEST_CASE(ExpectedLogFound, EspTestBase) {
    auto e = this->expectLog("foo");
    this->debug << "bar" << std::endl;
    this->debug << "foobar" << std::endl;
    this->debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
}

BOOST_FIXTURE_TEST_CASE(ExpectedNoLog, EspTestBase) {
    auto e = this->expectLog("baz", 0);
    this->debug << "bar" << std::endl;
    this->debug << "foobar" << std::endl;
    this->debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
}

BOOST_FIXTURE_TEST_CASE(ExpectedMultipleLogs, EspTestBase) {
    auto e = this->expectLog("foo", 3);
    this->debug << "foofoo" << std::endl;
    this->debug << "bar" << std::endl;
    this->debug << "foobar" << std::endl;
    this->debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
    this->debug << "barfoo" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END();
