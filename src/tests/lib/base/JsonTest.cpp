#include "TestSupport.hh"
#include "base/Json.hh"

namespace anchor::test {
using namespace testing;

TEST(Json, ParsesValidValuesAndReportsInvalidInput) {
    auto result = parseJson(R"({"key":[1,true,null]})");
    ASSERT_TRUE(result);
    EXPECT_EQ((*result)["key"], Json::array({1, true, nullptr}));
    auto null = parseJson("null");
    ASSERT_TRUE(null);
    EXPECT_TRUE(null->is_null());
    for (const auto* input : {"", "{", "[1,]", "{} trailing"}) {
        auto invalid = parseJson(input);
        ASSERT_FALSE(invalid) << input;
        EXPECT_EQ(invalid.error().code(), ErrorCode::jsonParsingError);
        EXPECT_THAT(
            invalid.error().message(), HasSubstr("Could not parse json")
        );
    }
}

}  // namespace anchor::test
