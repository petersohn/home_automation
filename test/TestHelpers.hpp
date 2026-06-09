#ifndef TEST_TESTHELPERS_HPP
#define TEST_TESTHELPERS_HPP

#include <gtest/gtest.h>

#include <algorithm>
#include <ostream>
#include <string>

template <typename IterA, typename IterB>
::testing::AssertionResult CollectionsEqual(
    IterA a_begin, IterA a_end, IterB b_begin, IterB b_end, const char* a_expr,
    const char* b_expr) {
    auto a_it = a_begin;
    auto b_it = b_begin;
    size_t index = 0;
    while (a_it != a_end && b_it != b_end) {
        if (!(*a_it == *b_it)) {
            return ::testing::AssertionFailure()
                   << "Collections differ at index " << index << ": " << a_expr
                   << "[" << index << "] = " << *a_it << ", " << b_expr << "["
                   << index << "] = " << *b_it;
        }
        ++a_it;
        ++b_it;
        ++index;
    }
    if (a_it != a_end) {
        return ::testing::AssertionFailure()
               << a_expr << " has " << (index + 1)
               << " extra elements starting at index " << index;
    }
    if (b_it != b_end) {
        return ::testing::AssertionFailure()
               << b_expr << " has " << (index + 1)
               << " extra elements starting at index " << index;
    }
    return ::testing::AssertionSuccess();
}

#define EXPECT_COLLECTIONS_EQ(a_begin, a_end, b_begin, b_end) \
    EXPECT_PRED_FORMAT4(::CollectionsEqual, a_begin, a_end, b_begin, b_end)

#endif  // TEST_TESTHELPERS_HPP
