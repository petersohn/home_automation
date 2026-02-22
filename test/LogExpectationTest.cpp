#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_suite.hpp>

#include "EspTestBase.hpp"

BOOST_AUTO_TEST_SUITE(LogExpectationTest)

BOOST_FIXTURE_TEST_CASE(ExpectedLogFound, EspTestBase) {
    auto e = expectLog("foo");
    debug << "bar" << std::endl;
    debug << "foobar" << std::endl;
    debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
}

BOOST_FIXTURE_TEST_CASE(ExpectedNoLog, EspTestBase) {
    auto e = expectLog("baz", 0);
    debug << "bar" << std::endl;
    debug << "foobar" << std::endl;
    debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
}

BOOST_FIXTURE_TEST_CASE(ExpectedMultipleLogs, EspTestBase) {
    auto e = expectLog("foo", 3);
    debug << "foofoo" << std::endl;
    debug << "bar" << std::endl;
    debug << "foobar" << std::endl;
    debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
    debug << "barfoo" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END();
