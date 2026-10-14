#include "TestSupport.hh"
#include "base/Error.hh"

namespace anchor::test {
using namespace testing;

TEST(Error, DefaultsFormattingAndReset) {
    Error error;
    EXPECT_FALSE(error.valid());
    EXPECT_EQ(error.code(), ErrorCode::noError);
    EXPECT_EQ(error.message(), "No details provided");
    error = Error{ErrorCode::invalidArgument, "bad value: {}", 42};
    EXPECT_TRUE(error.valid());
    EXPECT_EQ(error.message(), "bad value: 42");
    EXPECT_THAT(
        fmt::format("{}", error), HasSubstr("details: 'bad value: 42'")
    );
    error.reset();
    EXPECT_FALSE(error.valid());
    EXPECT_TRUE(error.message().empty());
    EXPECT_FALSE(Error::empty());
}

TEST(Error, UnexpectedPreservesCodeAndDetails) {
    Result<int> result =
        Error::unexpected(ErrorCode::invalidConfig, "missing {}", "port");
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::invalidConfig);
    EXPECT_EQ(result.error().message(), "missing port");
    Result<void> copy = Error::unexpected(result.error());
    EXPECT_EQ(copy.error().message(), result.error().message());
}

}  // namespace anchor::test
