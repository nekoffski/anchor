#include "TestSupport.hh"
#include "base/Time.hh"

namespace anchor::test {

TEST(Time, FormatsCalendarDateAndTime) {
    auto timestamp = std::chrono::sys_days{std::chrono::year{2026} / 10 / 14} +
                     12h + 34min + 56s;
    EXPECT_EQ(formatTime(timestamp, "%Y-%m-%d %H:%M"), "2026-10-14 12:34");
}

}  // namespace anchor::test
