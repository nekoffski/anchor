#include "TestSupport.hh"
#include "base/Env.hh"

namespace anchor::test {
using namespace testing;

TEST(ScopedEnv, RestoresValuesAfterSettingUnsettingAndEmptyValues) {
    ScopedEnv outer{"ANCHOR_TEST_ENV", "original"};
    {
        ScopedEnv unset{"ANCHOR_TEST_ENV"};
        EXPECT_FALSE(getEnv("ANCHOR_TEST_ENV"));
    }
    EXPECT_EQ(getEnv("ANCHOR_TEST_ENV"), "original");
    {
        ScopedEnv empty{"ANCHOR_TEST_ENV", ""};
        EXPECT_EQ(getEnv("ANCHOR_TEST_ENV"), "");
    }
    EXPECT_EQ(getEnv("ANCHOR_TEST_ENV"), "original");
}

TEST(Env, MissingInvalidAndOutOfRangeValuesReturnEmpty) {
    ScopedEnv missing{"ANCHOR_TEST_ENV"};
    EXPECT_FALSE(getEnv("ANCHOR_TEST_ENV"));
    {
        ScopedEnv invalid{"ANCHOR_TEST_ENV", "not-a-number"};
        EXPECT_FALSE(getEnv<i64>("ANCHOR_TEST_ENV"));
        EXPECT_FALSE(getEnv<u64>("ANCHOR_TEST_ENV"));
        EXPECT_FALSE(getEnv<f32>("ANCHOR_TEST_ENV"));
        EXPECT_FALSE(getEnv<f64>("ANCHOR_TEST_ENV"));
    }
    ScopedEnv overflow{
        "ANCHOR_TEST_ENV", "99999999999999999999999999999999999"
    };
    EXPECT_FALSE(getEnv<i64>("ANCHOR_TEST_ENV"));
    EXPECT_FALSE(getEnv<u64>("ANCHOR_TEST_ENV"));
}

TEST(Env, ConvertsStringsIntegersAndFloatingPointValues) {
    ScopedEnv value{"ANCHOR_TEST_ENV", "42"};
    EXPECT_EQ(getEnv("ANCHOR_TEST_ENV"), "42");
    EXPECT_EQ(getEnv<u64>("ANCHOR_TEST_ENV"), 42);
    EXPECT_EQ(getEnv<i64>("ANCHOR_TEST_ENV"), 42);
    EXPECT_EQ(getEnv<f32>("ANCHOR_TEST_ENV"), 42.0f);
    EXPECT_EQ(getEnv<f64>("ANCHOR_TEST_ENV"), 42.0);
    ScopedEnv negative{"ANCHOR_TEST_ENV", "-42"};
    EXPECT_EQ(getEnv<i64>("ANCHOR_TEST_ENV"), -42);
}

}  // namespace anchor::test
