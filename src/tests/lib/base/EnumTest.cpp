#include "TestSupport.hh"
#include "base/Enum.hh"

namespace anchor::test {
using namespace testing;

enum class Flags { none = 0, first = 1, second = 2 };
ANCHOR_BIT_ENUM(Flags)
TEST(Enum, CombinesAndChecksEnabledFlags) {
    auto flags = Flags::first | Flags::second;
    EXPECT_TRUE(checkFlag(flags, Flags::first));
    EXPECT_TRUE(checkFlag(flags, Flags::second));
    EXPECT_FALSE(checkFlag(Flags::first, Flags::second));
    EXPECT_EQ(flags & Flags::first, Flags::first);
    flags &= Flags::first;
    EXPECT_EQ(flags, Flags::first);
    flags |= Flags::second;
    EXPECT_TRUE(checkFlag(flags, Flags::second));
}

}  // namespace anchor::test
