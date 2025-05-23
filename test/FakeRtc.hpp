#ifndef TEST_FAKERTC_HPP
#define TEST_FAKERTC_HPP

#include <map>

#include "common/rtc.hpp"

class FakeRtc : public Rtc {
public:
    virtual Data get(unsigned id) override;
    virtual void set(unsigned id, Data value) override;
    virtual unsigned next() override;

    void reset();

private:
    std::map<unsigned, Data> data;
    unsigned current = 0;
};

#endif  // TEST_FAKERTC_HPP
