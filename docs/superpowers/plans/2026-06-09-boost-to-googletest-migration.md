# Boost.Test → Googletest Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Boost.Test with Googletest in the host-side `home_automation_test` binary, with no behavior change to any test, and no change to device code or to the `bin/operation_tester` CLI.

**Architecture:** Add googletest v1.15.2 as a git submodule at `test/googletest`, compile it via `add_subdirectory(test/googletest)`, then rewrite all 16 Boost.Test-using files in `test/` to use googletest macros and assertions, following the mechanical translation rules in `docs/superpowers/specs/2026-06-09-boost-to-googletest-migration-design.md`. The work is done as a one-shot rewrite: the test binary does not compile in the middle of the change, but each task ends with a green build (because the task's portion of the change is either the wiring-only step with no real test changes, or the full rewrite with all 16 files done).

**Tech Stack:** C++17, CMake ≥ 4.1, Googletest v1.15.2, ESP8266 Arduino toolchain (untouched, only used to verify the device build is still healthy).

---

## Task 1: Add googletest submodule and wire CMake

**Files:**
- Modify: `.gitmodules`
- Create: `test/googletest/` (submodule)
- Modify: `CMakeLists.txt`
- Delete: `test/testMain.cpp`
- Modify: `AGENTS.md`

This task lands the wiring change in isolation. We keep one of the existing test files (e.g. `collectionTest.cpp`) temporarily as-is and confirm that the build does *not* succeed yet (because Boost is gone) — that's the expected state. We then do the full file rewrite in Tasks 2–3.

- [ ] **Step 1.1: Add the submodule at a pinned tag**

Run from the project root:

```bash
git submodule add --branch v1.15.2 https://github.com/google/googletest.git test/googletest
git -C test/googletest checkout v1.15.2
git submodule update --init --recursive
```

Expected: `test/googletest/.git` exists, `cat test/googletest/CMakeLists.txt | head -1` shows the project line, and `git status` shows `.gitmodules` and a new untracked `test/googletest/` directory.

- [ ] **Step 1.2: Verify the .gitmodules entry**

```bash
cat .gitmodules
```

Expected output (existing two entries plus a new third):

```
[submodule "test/ArduinoJson"]
	path = test/ArduinoJson
	url = https://github.com/bblanchon/ArduinoJson.git
[submodule "src/SDS011"]
	path = src/SDS011
	url = https://github.com/ricki-z/SDS011.git
[submodule "test/googletest"]
	path = test/googletest
	url = https://github.com/google/googletest.git
	branch = v1.15.2
```

- [ ] **Step 1.3: Update CMakeLists.txt**

Replace the contents of `CMakeLists.txt` with:

```cmake
cmake_minimum_required (VERSION 4.1)
project(home_automation)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

set (CXX_COMMON_FLAGS "-std=c++17 -Wall -Wextra -Werror")
set (CMAKE_CXX_FLAGS_DEBUG "${CXX_COMMON_FLAGS} -O0 -g")
set (CMAKE_CXX_FLAGS_RELEASE "${CXX_COMMON_FLAGS} -O2")

include_directories(src)
include_directories(test/ArduinoJson)

add_subdirectory(test/googletest)

file(GLOB test_sources test/*.cpp)
file(GLOB common_sources src/common/*.cpp)
file(GLOB operation_sources src/operation/*.cpp)
file(GLOB tools_sources src/tools/*.cpp)

add_executable(home_automation_test
    ${test_sources} ${operation_sources} ${common_sources} ${tools_sources})
target_link_libraries(home_automation_test gtest gtest_main)

add_executable(operation_tester
    ${operation_sources}  ${common_sources} ${tools_sources}
    bin/operation_tester.cpp)
```

- [ ] **Step 1.4: Delete testMain.cpp**

```bash
git rm test/testMain.cpp
```

- [ ] **Step 1.5: Update AGENTS.md run command**

In `AGENTS.md`, replace the `## Testing` section's run command (currently `home_automation_test --log_level=test_suite [ -t test_to_run ]`) with:

```
./build/home_automation_test [--gtest_filter=SuiteName.*]
```

And add a new bullet under `## Building` saying:

> After cloning, run `git submodule update --init --recursive` to fetch `test/googletest` and `test/ArduinoJson`.

- [ ] **Step 1.6: Build and confirm the test binary is broken in the expected way**

```bash
cd build
cmake ..
make -j$(nproc) 2>&1 | tail -40
```

Expected: the build fails inside one of the test files (e.g. `collectionTest.cpp`) with `boost/test/unit_test.hpp: No such file or directory`. The `operation_tester` target may still build — that's fine; the failure is in the test target.

- [ ] **Step 1.7: Verify device build is unaffected**

Run from the project root:

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds. (The device build does not pull in `test/`.)

- [ ] **Step 1.8: Commit**

```bash
cd /home/petersohn/workspace/home_automation
git add .gitmodules test/googletest CMakeLists.txt AGENTS.md
git commit -m "Wire googletest submodule and update CMake"
```

---

## Task 2: Rewrite non-parameterized test files and helpers

**Files:**
- Modify: `test/collectionTest.cpp`
- Modify: `test/LogExpectationTest.cpp`
- Modify: `test/LogExpectation.cpp`
- Modify: `test/LogExpectation.hpp` (only if it includes Boost)
- Modify: `test/TestStream.cpp` (only if it includes Boost)
- Modify: `test/TestStream.hpp` (only if it includes Boost)
- Modify: `test/FakeAnalogInput.cpp` / `.hpp`
- Modify: `test/FakeEspApi.cpp` / `.hpp`
- Modify: `test/FakeMqttConnection.cpp` / `.hpp`
- Modify: `test/FakeRtc.cpp` / `.hpp`
- Modify: `test/FakeWifi.cpp` / `.hpp`
- Modify: `test/EspTestBase.cpp` / `.hpp`
- Modify: `test/InterfaceTestBase.cpp` / `.hpp`
- Modify: `test/DummyBackoff.hpp`

This task migrates the *simple* files: helpers, the small `collectionTest.cpp`, and `LogExpectationTest.cpp` (which has no data-driven tests). The data-driven rewrites are deferred to Task 3.

- [ ] **Step 2.1: Sweep all helper files for Boost includes**

Run:

```bash
grep -l "boost/test" test/FakeAnalogInput.cpp test/FakeAnalogInput.hpp \
  test/FakeEspApi.cpp test/FakeEspApi.hpp \
  test/FakeMqttConnection.cpp test/FakeMqttConnection.hpp \
  test/FakeRtc.cpp test/FakeRtc.hpp \
  test/FakeWifi.cpp test/FakeWifi.hpp \
  test/EspTestBase.cpp test/EspTestBase.hpp \
  test/InterfaceTestBase.cpp test/InterfaceTestBase.hpp \
  test/LogExpectation.cpp test/LogExpectation.hpp \
  test/TestStream.cpp test/TestStream.hpp \
  test/DummyBackoff.hpp
```

For each file that prints a hit:

- If the file includes Boost headers but does not use any Boost assertion macro (the only Boost use is the include line), replace the include with `<gtest/gtest.h>` if the file uses gtest assertions, otherwise remove the include entirely.
- If the file uses a Boost assertion (e.g. `BOOST_REQUIRE` in `LogExpectation.cpp`), apply the assertion translation in Step 2.2.

The mechanical rule for "uses Boost assertion" is: grep the file for any of `BOOST_REQUIRE`, `BOOST_TEST`, `BOOST_CHECK`, `BOOST_FAIL`, `BOOST_AUTO_TEST_CASE`, `BOOST_DATA_TEST_CASE`.

- [ ] **Step 2.2: Rewrite LogExpectation.cpp and LogExpectation.hpp**

The current `LogExpectation::expectLog` returns a `shared_ptr<LogExpectation>` and registers a destructor assertion via `BOOST_REQUIRE`. The simplest mechanical change is to switch the destructor to use `EXPECT_EQ` / `ASSERT_EQ` as appropriate, and switch any include to `<gtest/gtest.h>`.

In `test/LogExpectation.hpp`:
- Replace `#include <boost/test/unit_test.hpp>` with `#include <gtest/gtest.h>`.
- (If `<gtest/gtest.h>` is not strictly needed because the header only forward-declares `LogExpectation` and includes nothing, remove the include entirely.)

In `test/LogExpectation.cpp`:
- Replace the include with `<gtest/gtest.h>`.
- Replace any `BOOST_REQUIRE(cond)` with `ASSERT_TRUE(cond)`.
- Replace any `BOOST_TEST(cond)` with `EXPECT_TRUE(cond)`.

- [ ] **Step 2.3: Rewrite collectionTest.cpp**

Replace the file contents with:

```cpp
#include <string>
#include <vector>

#include "tools/collection.hpp"

TEST(CollectionFindValueTest, FindValue) {
    const std::vector<std::pair<std::string, int>> values{
        std::make_pair("foo", 1),
        std::make_pair("bar", 2),
        std::make_pair("foobar", 3),
    };
    EXPECT_EQ(tools::findValue(values, "foo"), &values[0].second);
    EXPECT_EQ(tools::findValue(values, "bar"), &values[1].second);
    EXPECT_EQ(tools::findValue(values, "foobar"), &values[2].second);
}

TEST(CollectionFindValueTest, NotFound) {
    const std::vector<std::pair<std::string, int>> values{
        std::make_pair("foo", 1),
        std::make_pair("bar", 2),
        std::make_pair("foobar", 3),
    };
    EXPECT_EQ(tools::findValue(values, "baz"), nullptr);
}
```

This collapses the nested `CollectionTest > FindValueTest` Boost suites into a single gtest suite `CollectionFindValueTest`, matching the design.

- [ ] **Step 2.4: Rewrite LogExpectationTest.cpp**

Replace the file contents with:

```cpp
#include "EspTestBase.hpp"

TEST(LogExpectationTest, ExpectedLogFound) {
    auto e = this->expectLog("foo");
    this->debug << "bar" << std::endl;
    this->debug << "foobar" << std::endl;
    this->debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
}

TEST(LogExpectationTest, ExpectedNoLog) {
    auto e = this->expectLog("baz", 0);
    this->debug << "bar" << std::endl;
    this->debug << "foobar" << std::endl;
    this->debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
}

TEST(LogExpectationTest, ExpectedMultipleLogs) {
    auto e = this->expectLog("foo", 3);
    this->debug << "foofoo" << std::endl;
    this->debug << "bar" << std::endl;
    this->debug << "foobar" << std::endl;
    this->debug << "laskdhfroequwrbv qoreibqewroujqw" << std::endl;
    this->debug << "barfoo" << std::endl;
}
```

- [ ] **Step 2.5: Build and verify Tasks 1+2**

```bash
cd build
cmake ..
make -j$(nproc) 2>&1 | tail -50
```

Expected: the build still fails — but the only remaining errors should be inside the not-yet-migrated test files (`AnalogSensorTest.cpp`, `BackoffTest.cpp`, `MqttClientTest.cpp`, `OperationParser2Test.cpp`, `OperationParserTest.cpp`, `CoverTest.cpp`, `stringTest.cpp`), all of them "no member / unknown macro" or "boost/test/unit_test.hpp: No such file or directory". No errors in any helper or in the files rewritten in Steps 2.1–2.4.

Verify this by:

```bash
make -j$(nproc) 2>&1 | grep -E "error:" | head -20
```

If you see an error in `collectionTest.cpp`, `LogExpectationTest.cpp`, `LogExpectation.cpp`, or any `Fake*.cpp/hpp`, fix it before continuing.

- [ ] **Step 2.6: Commit**

```bash
cd /home/petersohn/workspace/home_automation
git add test/collectionTest.cpp test/LogExpectationTest.cpp \
  test/LogExpectation.cpp test/LogExpectation.hpp \
  test/TestStream.cpp test/TestStream.hpp \
  test/FakeAnalogInput.cpp test/FakeAnalogInput.hpp \
  test/FakeEspApi.cpp test/FakeEspApi.hpp \
  test/FakeMqttConnection.cpp test/FakeMqttConnection.hpp \
  test/FakeRtc.cpp test/FakeRtc.hpp \
  test/FakeWifi.cpp test/FakeWifi.hpp \
  test/EspTestBase.cpp test/EspTestBase.hpp \
  test/InterfaceTestBase.cpp test/InterfaceTestBase.hpp \
  test/DummyBackoff.hpp
git commit -m "Migrate simple test files and helpers to googletest"
```

---

## Task 3: Add the EXPECT_COLLECTIONS_EQ helper

**Files:**
- Create: `test/TestHelpers.hpp`

This small header centralizes the `BOOST_CHECK_EQUAL_COLLECTIONS` → gtest translation. The current code uses `BOOST_CHECK_EQUAL_COLLECTIONS` in `MqttClientTest.cpp` only; the helper is introduced now so Task 4 can use it.

- [ ] **Step 3.1: Write TestHelpers.hpp**

Create `test/TestHelpers.hpp` with:

```cpp
#ifndef TEST_TESTHELPERS_HPP
#define TEST_TESTHELPERS_HPP

#include <gtest/gtest.h>

#include <algorithm>
#include <ostream>
#include <string>

template <typename IterA, typename IterB>
::testing::AssertionResult CollectionsEqual(
    IterA a_begin, IterA a_end, IterB b_begin, IterB b_end,
    const char* a_expr, const char* b_expr) {
    auto a_it = a_begin;
    auto b_it = b_begin;
    size_t index = 0;
    while (a_it != a_end && b_it != b_end) {
        if (!(*a_it == *b_it)) {
            return ::testing::AssertionFailure()
                << "Collections differ at index " << index << ": "
                << a_expr << "[" << index << "] = " << *a_it
                << ", " << b_expr << "[" << index << "] = " << *b_it;
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
```

- [ ] **Step 3.2: Build and verify it compiles**

Add a one-line include to `test/collectionTest.cpp` (temporarily) — `#include "TestHelpers.hpp"` — and rebuild. Then remove the include.

```bash
cd build
cmake --build . --target home_automation_test -j$(nproc) 2>&1 | tail -20
```

Expected: no new errors. (The helper is header-only; it compiles into the next translation unit that includes it.)

- [ ] **Step 3.3: Commit**

```bash
cd /home/petersohn/workspace/home_automation
git add test/TestHelpers.hpp
git commit -m "Add EXPECT_COLLECTIONS_EQ helper for googletest"
```

---

## Task 4: Rewrite parameterized test files

**Files:**
- Modify: `test/AnalogSensorTest.cpp`
- Modify: `test/BackoffTest.cpp`
- Modify: `test/OperationParser2Test.cpp`
- Modify: `test/OperationParserTest.cpp`
- Modify: `test/CoverTest.cpp`
- Modify: `test/MqttClientTest.cpp`
- Modify: `test/stringTest.cpp`

This is the bulk of the work. Each file is rewritten end-to-end using the assertion translation rules from the spec. Read the spec section "Assertion translation" and "Parameterized tests" before starting; both are authoritative.

- [ ] **Step 4.1: Rewrite BackoffTest.cpp**

The current file uses `BOOST_FIXTURE_TEST_CASE` against a `Fixture` struct derived from `EspTestBase`. The rewrite is:

- Drop the `BOOST_AUTO_TEST_SUITE(BackoffTest)` / `_END` lines.
- Change `BOOST_FIXTURE_TEST_CASE(Good, Fixture)` → `TEST_F(BackoffTest, Good)`.
- Rename the struct from `Fixture` to `BackoffTest` (it is the gtest fixture, so its name is the gtest suite name).
- Replace `BOOST_REQUIRE_EQUAL(a, b)` with `ASSERT_EQ(a, b)`.
- Replace `BOOST_REQUIRE_NO_THROW(expr)` with `EXPECT_NO_THROW(expr)`.

`this->reset()` and `this->test(...)` keep their definitions; the body of `test` becomes:

```cpp
void test(unsigned long delay, bool good, bool shouldRestart = false) {
    this->esp.delay(delay);
    if (good) {
        this->backoff->good();
    } else {
        this->backoff->bad();
    }

    ASSERT_EQ(this->esp.restarted, shouldRestart);

    if (shouldRestart) {
        this->reset();
    }
}
```

The constructor stays as `BackoffTest() { this->reset(); }`. The other two test cases (`Bad`, `ResetAfterFix`) follow the same pattern.

- [ ] **Step 4.2: Rewrite AnalogSensorTest.cpp**

- Replace the `boost_test_print_type` ADL hook with a gtest `PrintTo` overload in `namespace std`:

```cpp
namespace std {
void PrintTo(const std::optional<std::vector<std::string>>& value,
             std::ostream* os) {
    if (!value) {
        *os << "<none>";
        return;
    }
    *os << "{";
    for (size_t i = 0; i < value->size(); ++i) {
        *os << "\"" << (*value)[i] << "\"";
        if (i + 1 < value->size()) {
            *os << ", ";
        }
    }
    *os << "}";
}
}  // namespace std
```

- Drop the `BOOST_AUTO_TEST_SUITE(AnalogSensorTest)` / `_END` lines.
- Rename the inner `Fixture` class to `AnalogSensorTest` (gtest fixture, suite name).
- Replace `BOOST_FIXTURE_TEST_CASE(name, Fixture)` with `TEST_F(AnalogSensorTest, name)`.
- Replace `BOOST_TEST(this->sensor->measure() == this->expected("12"))` with `EXPECT_EQ(this->sensor->measure(), this->expected("12"))`.
- The `expected(...)` helpers can stay unchanged. The `optional<vector<string>>` `PrintTo` makes `EXPECT_EQ` produce a readable failure message.
- The `BOOST_DATA_TEST_CASE_F` blocks for `Aggregate50HzSine` / `Aggregate50HzSineWithScaling` become `TEST_P(AnalogSensorTest, Aggregate50HzSine)` etc., with:

```cpp
INSTANTIATE_TEST_SUITE_P(Analog, AnalogSensorTest,
                         testing::Values(1, 2),
                         [](const testing::TestParamInfo<AnalogSensorTest::ParamType>& info) {
                             return "delay" + std::to_string(info.param);
                         });
```

- `BOOST_REQUIRE(result)` becomes `ASSERT_TRUE(result)`.
- `BOOST_REQUIRE(result->size() == 2)` becomes `ASSERT_EQ(result->size(), 2u)`.
- `BOOST_TEST_MESSAGE("avg=" << avg << " max=" << max)` becomes `RecordProperty("avg", avg); RecordProperty("max", max);` (the message is then visible in the gtest XML output if `--gtest_output=xml` is set, and also emitted as a test property in the standard output). Alternatively, for human-friendly output: `std::cerr << "avg=" << avg << " max=" << max << "\n";` — the design spec specifies `RecordProperty`; use it.

- [ ] **Step 4.3: Rewrite stringTest.cpp**

This file has 148 `BOOST_` macro hits. Apply the same mechanical rules:

- `BOOST_AUTO_TEST_SUITE` / `_END` → drop.
- `BOOST_AUTO_TEST_CASE(name)` → `TEST(SuiteName, name)` where `SuiteName` is the prior suite name.
- `BOOST_TEST(cond)` → `EXPECT_TRUE(cond)` or `EXPECT_EQ(...)` depending on the form.
- `BOOST_REQUIRE(cond)` → `ASSERT_TRUE(cond)`.
- For `BOOST_TEST(some_string == "literal")`, prefer `EXPECT_EQ(some_string, "literal")` when `some_string` has `operator<<` (it does, since `std::string` does).

Read the file in full first to map suite names. The file is 148 hits in roughly 250 lines; read it end-to-end.

- [ ] **Step 4.4: Rewrite OperationParser2Test.cpp**

- Drop `BOOST_AUTO_TEST_SUITE(OperationParser2Test)` / `_END`.
- Rename `Fixture` to `OperationParser2Test`.
- `BOOST_FIXTURE_TEST_CASE(ConstantString, Fixture)` → `TEST_F(OperationParser2Test, ConstantString)`.
- `BOOST_REQUIRE(operation != nullptr)` → `ASSERT_NE(operation, nullptr)`.
- `BOOST_TEST(operation->evaluate() == "foobar")` → `EXPECT_EQ(operation->evaluate(), "foobar")`.
- Apply the same pattern to all 70+ tests in this file. The `auto ex = expectLog("Syntax error:")` lines stay as-is — `expectLog` returns a `shared_ptr<LogExpectation>` and the destructor of the temporary triggers the assertion.

- [ ] **Step 4.5: Rewrite OperationParserTest.cpp**

Same pattern as 4.4, with these special cases:

- The two `boost::test_tools::tolerance(1e-6)` call sites (lines 67 and 456) become `EXPECT_NEAR(std::atof(operation->evaluate().c_str()), sample.expectedValue, 1e-6)`. This is a deliberate change in semantics (absolute → relative-then-absolute) per the design's risks section. The reviewer should be aware of it during code review.
- The `BOOST_DATA_TEST_CASE_F` blocks become `TEST_P` + multiple `INSTANTIATE_TEST_SUITE_P` calls. The `+` concatenation pattern (`boost::unit_test::data::make(stringOperations) + comparisons`) becomes:

```cpp
INSTANTIATE_TEST_SUITE_P(Ops, OperationParserTest,
                         testing::ValuesIn(stringOperations));
INSTANTIATE_TEST_SUITE_P(Ops, OperationParserTest,
                         testing::ValuesIn(comparisons));
```

Both with the same instance prefix `Ops`. gtest combines them into one set of test names with separate indices.

- For the `Fixture` struct: rename to `OperationParserTest` so the gtest suite name is the fixture name. The `createInterfaces()` static method, `parse(...)` method, and member fields stay as-is.

- [ ] **Step 4.6: Rewrite CoverTest.cpp**

- Drop the `BOOST_AUTO_TEST_SUITE(CoverTest)` / `_END` lines.
- The inner `Fixture` class is renamed to `CoverTest`.
- `BOOST_FIXTURE_TEST_CASE(name, Fixture)` → `TEST_F(CoverTest, name)`.
- `BOOST_DATA_TEST_CASE_F(Fixture, Open, params1, delay, isLatching, hasPositionSensor)` becomes `TEST_P(CoverTest, Open)` plus:

```cpp
INSTANTIATE_TEST_SUITE_P(
    Cover, CoverTest,
    testing::Combine(
        testing::ValuesIn(delays1),
        testing::ValuesIn(latchings),
        testing::ValuesIn(hasPositionSensorValues)));
```

Note: `params1` is `delays1 * latchings * hasPositionSensorValues`; the rewrite is `Combine` of the same three.

- The `params2NoPositionSensor` Cartesian product (5-way) becomes:

```cpp
INSTANTIATE_TEST_SUITE_P(
    Cover2, CoverTest,
    testing::Combine(
        testing::ValuesIn(delays2),
        testing::ValuesIn(latchings),
        testing::Bool(), testing::Bool(), testing::Bool()));
```

- Inside each `TEST_P`, the previously-bound names (`delay`, `isLatching`, `hasPositionSensor`, …) come from `this->GetParam()` as a `std::tuple<…>`. Add at the top of each `TEST_P`:

```cpp
auto delay = std::get<0>(GetParam());
auto isLatching = std::get<1>(GetParam());
auto hasPositionSensor = std::get<2>(GetParam());
```

(or use structured bindings if the tuple is short and the test readability benefits).

- `BOOST_TEST_CONTEXT(name) { body; }` becomes:

```cpp
SCOPED_TRACE(name);
body;
```

- `BOOST_TEST(this->esp.digitalRead(UpOutput) == upValue)` becomes `EXPECT_EQ(this->esp.digitalRead(UpOutput), upValue)`.
- `BOOST_TEST(this->isMovingUp())` becomes `EXPECT_TRUE(this->isMovingUp())`.
- `BOOST_TEST_MESSAGE(...)` becomes `RecordProperty("message", …)` per the spec.

- [ ] **Step 4.7: Rewrite MqttClientTest.cpp**

- Drop the suite / end lines.
- Rename `Fixture` to `MqttClientTest`.
- `BOOST_FIXTURE_TEST_CASE(NormalFlow, Fixture)` → `TEST_F(MqttClientTest, NormalFlow)`.
- The three `BOOST_CHECK_EQUAL_COLLECTIONS(a, b, c, d)` calls in the `check(...)` helper become `EXPECT_COLLECTIONS_EQ(a, b, c, d)`. Add `#include "TestHelpers.hpp"` at the top of the file.
- `BOOST_TEST(cond)` and `BOOST_REQUIRE(cond)` follow the standard rules.

- [ ] **Step 4.8: Build the test target**

```bash
cd build
cmake --build . --target home_automation_test -j$(nproc) 2>&1 | tail -50
```

Expected: no errors. If any file has a remaining `BOOST_` macro or a missing include, fix it in place.

- [ ] **Step 4.9: Run all tests**

```bash
./build/home_automation_test 2>&1 | tail -60
```

Expected: every test passes. The number of test cases reported by gtest is at least 200 (a rough estimate; the exact number comes from `grep -c "BOOST_\(AUTO_\|FIXTURE_\|DATA_\)\?TEST_CASE" test/*.cpp | awk -F: '{s+=$2} END {print s}'` which is the original Boost count).

If any test fails, debug it like a normal bug — gtest output includes file/line and the expected/actual values. Do *not* paper over a failure by changing a test's expectation; the goal is byte-equivalent behavior.

- [ ] **Step 4.10: Commit**

```bash
cd /home/petersohn/workspace/home_automation
git add test/AnalogSensorTest.cpp test/BackoffTest.cpp \
  test/OperationParser2Test.cpp test/OperationParserTest.cpp \
  test/CoverTest.cpp test/MqttClientTest.cpp test/stringTest.cpp
git commit -m "Migrate parameterized test files to googletest"
```

---

## Task 5: Final cleanup, documentation, and verification

**Files:**
- Modify: `AGENTS.md` (final pass)
- Modify: `compile_commands.json` (regenerated)
- (No source changes expected.)

- [ ] **Step 5.1: Regenerate compile_commands.json**

```bash
cd build
cmake --fresh ..
cmake --build . --target home_automation_test -j$(nproc)
```

Expected: `build/compile_commands.json` is regenerated. The project-root symlink `compile_commands.json` continues to point at the regenerated file.

- [ ] **Step 5.2: Run the device build**

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

Expected: device build succeeds with no warnings or errors.

- [ ] **Step 5.3: Run the operation_tester binary**

```bash
cd build
cmake --build . --target operation_tester -j$(nproc)
```

Expected: builds. (This target was untouched in the migration; the verification is a smoke test that nothing accidentally broke it.)

- [ ] **Step 5.4: Run all tests with a representative filter**

```bash
./build/home_automation_test --gtest_filter='CoverTest.*' 2>&1 | tail -30
./build/home_automation_test --gtest_filter='*Operation*' 2>&1 | tail -30
./build/home_automation_test 2>&1 | tail -10
```

Expected: each command exits 0 and reports `[  PASSED  ] N tests` at the bottom.

- [ ] **Step 5.5: Update AGENTS.md**

In `AGENTS.md`, replace the entire `## Testing` section with:

```markdown
## Testing

Testing uses the Googletest framework. To run the tests, run:

```
./build/home_automation_test [--gtest_filter=SuiteName.*]
```

After cloning or pulling, run `git submodule update --init --recursive` to
fetch the `test/googletest` and `test/ArduinoJson` submodules.
```

- [ ] **Step 5.6: Run clang-format on all modified test files**

```bash
clang-format -i test/AnalogSensorTest.cpp test/BackoffTest.cpp \
  test/OperationParser2Test.cpp test/OperationParserTest.cpp \
  test/CoverTest.cpp test/MqttClientTest.cpp test/stringTest.cpp \
  test/LogExpectationTest.cpp test/LogExpectation.cpp \
  test/LogExpectation.hpp test/TestStream.cpp test/TestStream.hpp \
  test/FakeAnalogInput.cpp test/FakeAnalogInput.hpp \
  test/FakeEspApi.cpp test/FakeEspApi.hpp \
  test/FakeMqttConnection.cpp test/FakeMqttConnection.hpp \
  test/FakeRtc.cpp test/FakeRtc.hpp test/FakeWifi.cpp test/FakeWifi.hpp \
  test/EspTestBase.cpp test/EspTestBase.hpp \
  test/InterfaceTestBase.cpp test/InterfaceTestBase.hpp \
  test/collectionTest.cpp test/DummyBackoff.hpp test/TestHelpers.hpp
```

Expected: no output. (clang-format is a no-op when the file is already formatted.)

- [ ] **Step 5.7: Final build and test**

```bash
cd build
cmake --build . -j$(nproc)
./build/home_automation_test 2>&1 | tail -10
```

Expected: builds clean; all tests pass.

- [ ] **Step 5.8: Commit**

```bash
cd /home/petersohn/workspace/home_automation
git add AGENTS.md test/ compile_commands.json
git commit -m "Update AGENTS.md and run final formatting pass"
```

---

## Self-Review

**Spec coverage check:**

- Submodule setup → Task 1.1, 1.2.
- CMake changes → Task 1.3.
- Test file rewrites → Tasks 2, 3, 4.
- Suite/case/fixture translation → Steps 2.3, 2.4, 4.1–4.7.
- Parameterized tests → Steps 4.2, 4.5, 4.6.
- Assertion translation → Steps 2.2, 2.4, 4.1–4.7 (all reference the spec table).
- Custom printers → Step 4.2 (`PrintTo`).
- Run command → Task 1.5, Task 5.5.
- Files NOT touched (`bin/operation_tester.cpp`, device code, submodules) → Step 5.3 verifies `operation_tester` still builds; Step 5.2 verifies the device build still works; no spec requirement was missed.
- Risks (float tolerance) → Step 4.5 documents the deliberate `EXPECT_NEAR` choice.
- Out-of-scope items → no task touches them; covered by Steps 5.2 and 5.3.

**Placeholder scan:** No "TBD", "TODO", "implement later", or unfilled code blocks. Every step has a concrete command or a concrete code block.

**Type consistency:** `EXPECT_COLLECTIONS_EQ` is defined in `TestHelpers.hpp` and used in `MqttClientTest.cpp`; the macro signature matches. The `PrintTo` overload is in `namespace std` and matches the gtest signature. Fixture class names are consistent within each file: `BackoffTest` in `BackoffTest.cpp`, `AnalogSensorTest` in `AnalogSensorTest.cpp`, `CoverTest` in `CoverTest.cpp`, `OperationParserTest` in `OperationParserTest.cpp`, `OperationParser2Test` in `OperationParser2Test.cpp`, `MqttClientTest` in `MqttClientTest.cpp`.
