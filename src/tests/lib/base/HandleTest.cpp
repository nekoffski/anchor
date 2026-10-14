#include "TestSupport.hh"
#include "base/Handle.hh"

namespace anchor::test {
using namespace testing;

TEST(Handle, ValidityEqualityOrderingAndInvalidation) {
    HandleKey empty;
    EXPECT_FALSE(empty.valid());
    HandleKey first{1, 2};
    EXPECT_TRUE(first.valid());
    EXPECT_EQ(first, (HandleKey{1, 2}));
    EXPECT_NE(first, (HandleKey{1, 3}));
    EXPECT_LT(first, (HandleKey{2, 0}));
    EXPECT_LT(first, (HandleKey{1, 3}));
    first.invalidate();
    EXPECT_EQ(first, empty);
    using Handle = HandleBase<int, 7>;
    EXPECT_EQ(Handle::type(), 7);
    EXPECT_LT(Handle{1}, Handle{2});
    EXPECT_EQ(Handle{1}, Handle{1});
}

}  // namespace anchor::test
