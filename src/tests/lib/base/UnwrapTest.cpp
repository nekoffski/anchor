#include "TestSupport.hh"
#include "base/Unwrap.hh"

namespace anchor::test {
using namespace testing;

TEST(Unwrap, MovesSuccessfulValues) {
    Result<std::unique_ptr<int>> result{std::make_unique<int>(42)};
    auto value = unwrap(std::move(result));
    EXPECT_EQ(*value, 42);
}

TEST(UnwrapDeathTest, AbortsOnError) {
    EXPECT_DEATH(
        unwrap(
            Result<int>{Error::unexpected(ErrorCode::invalidArgument, "bad")}
        ),
        ""
    );
}

}  // namespace anchor::test
