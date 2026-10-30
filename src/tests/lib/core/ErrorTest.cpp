#include "TestSupport.hh"
#include "core/Error.hh"

namespace anchor::test {
using namespace testing;

TEST(ProjectError, SharesResultTypeAndPropagatesLibraryErrors) {
    static_assert(std::is_same_v<Result<int>, kc::Result<int>>);
    auto read = []() -> kc::Result<int> {
        return kc::Error::unexpected(
            kc::ErrorCode::fileNotFound, "Missing {}", "plan"
        );
    };
    auto load = [&]() -> Result<void> {
        auto value = read();
        if (!value) {
            return Error::unexpected(value.error());
        }
        return {};
    };
    const auto result = load();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), kc::ErrorCode::fileNotFound);
    EXPECT_EQ(result.error().message(), "Missing plan");
}

TEST(ProjectError, DistinguishesEqualNumericCodesAndFormatsBothCategories) {
    const Error project{ErrorCode::invalidRequest, "Bad request"};
    const Error library{kc::ErrorCode::internalError, "Library failure"};
    EXPECT_EQ(project.code().value(), library.code().value());
    EXPECT_NE(project.code(), library.code());
    EXPECT_EQ(project.code(), ErrorCode::invalidRequest);
    EXPECT_EQ(library.code(), kc::ErrorCode::internalError);
    EXPECT_EQ(&project.code().category(), &errorCategory());
    EXPECT_EQ(&library.code().category(), &kc::errorCategory());
    EXPECT_EQ(
        fmt::format("{}", project), "code: anchor:1, details: 'Bad request'"
    );
    EXPECT_EQ(
        fmt::format("{}", library), "code: kcore:1, details: 'Library failure'"
    );
}

}  // namespace anchor::test
