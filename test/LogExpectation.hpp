#ifndef LOG_EXPECTATION_HPP
#define LOG_EXPECTATION_HPP

#include <string>

class LogExpectation {
public:
    explicit LogExpectation(std::string expectedLog, std::size_t expectedCount)
        : expectedLog(std::move(expectedLog)), expectedCount(expectedCount) {}
    ~LogExpectation();
    void addLog(const std::string& log);

private:
    const std::string expectedLog;
    const std::size_t expectedCount;
    std::size_t count = 0;
};

#endif  // LOG_EXPECTATION_HPP
