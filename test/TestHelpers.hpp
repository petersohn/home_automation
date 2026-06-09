#ifndef TEST_TESTHELPERS_HPP
#define TEST_TESTHELPERS_HPP

#include <gtest/gtest.h>

#include <cstddef>

template <typename IterA, typename IterB>
::testing::AssertionResult CollectionsEqual(
    const char* a_begin_expr, const char* a_end_expr, const char* b_begin_expr,
    const char* b_end_expr, IterA a_begin, IterA a_end, IterB b_begin,
    IterB b_end) {
    auto a_it = a_begin;
    auto b_it = b_begin;
    std::size_t index = 0;
    while (a_it != a_end && b_it != b_end) {
        if (!(*a_it == *b_it)) {
            return ::testing::AssertionFailure()
                   << "Collections differ at index " << index << ": "
                   << a_begin_expr << "[" << index << "] = " << *a_it << ", "
                   << b_begin_expr << "[" << index << "] = " << *b_it;
        }
        ++a_it;
        ++b_it;
        ++index;
    }
    if (a_it != a_end) {
        return ::testing::AssertionFailure()
               << a_begin_expr << " has extra elements starting at index "
               << index;
    }
    if (b_it != b_end) {
        return ::testing::AssertionFailure()
               << b_begin_expr << " has extra elements starting at index "
               << index;
    }
    return ::testing::AssertionSuccess();
}

#define EXPECT_COLLECTIONS_EQ(a_begin, a_end, b_begin, b_end) \
    EXPECT_PRED_FORMAT4(::CollectionsEqual, a_begin, a_end, b_begin, b_end)

#endif  // TEST_TESTHELPERS_HPP
