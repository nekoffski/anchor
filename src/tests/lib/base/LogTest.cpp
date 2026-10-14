#include "LogCapture.hh"
#include "TestSupport.hh"

namespace anchor::test {
using namespace testing;

TEST(Log, ConvertsEveryLevelAndFallsBackForUnknownValues) {
    ASSERT_EQ(log::levelMap().size(), 7);
    for (const auto& [text, level] : log::levelMap()) {
        EXPECT_EQ(log::levelFromString(text), level);
        EXPECT_EQ(log::levelToString(level), text);
    }
    EXPECT_EQ(log::levelFromString("unknown"), log::Level::info);
    EXPECT_EQ(log::levelFromString("INFO"), log::Level::info);
    EXPECT_EQ(log::levelToString(static_cast<log::Level>(999)), "unknown");
}

using LogTest = LogCapture;

TEST_F(LogTest, SettingLogLevelUpdatesReportedAndSpdlogLevels) {
    for (const auto& [level, expected] :
         std::vector<std::pair<log::Level, spdlog::level::level_enum>>{
             {log::Level::off, spdlog::level::off},
             {log::Level::critical, spdlog::level::critical},
             {log::Level::error, spdlog::level::err},
             {log::Level::warn, spdlog::level::warn},
             {log::Level::info, spdlog::level::info},
             {log::Level::debug, spdlog::level::debug},
             {log::Level::trace, spdlog::level::trace}
         }) {
        log::setLogLevel(level);
        EXPECT_EQ(log::level(), level);
        EXPECT_EQ(spdlog::default_logger()->level(), expected);
    }
}

TEST_F(LogTest, FormatsMessagesAndFiltersBelowConfiguredLevel) {
    log::trace("trace {}", 1);
    log::debug("debug {}", 2);
    log::info("info {}", 3);
    log::warn("warn {}", 4);
    log::error("error {}", 5);
    EXPECT_EQ(output.str(), "trace 1\ndebug 2\ninfo 3\nwarn 4\nerror 5\n");
    log::setLogLevel(log::Level::warn);
    log::info("hidden");
    log::warn("visible");
    EXPECT_THAT(output.str(), Not(HasSubstr("hidden")));
    EXPECT_THAT(output.str(), HasSubstr("visible"));
}

TEST_F(LogTest, InitializationHonorsRequestedLevelWithAndWithoutColors) {
    for (bool colors : {false, true}) {
        log::init(
            {.enableColors = colors,
             .level = log::Level::error,
             .formatPattern = "%v"}
        );
        EXPECT_EQ(log::level(), log::Level::error);
        EXPECT_EQ(spdlog::default_logger()->level(), spdlog::level::err);
    }
}

TEST(LogDeathTest, FailedExpectAndPanicAbort) {
    log::expect(true, "fine");
    EXPECT_DEATH(log::expect(false, "failed"), "");
    EXPECT_DEATH(log::panic("panic"), "");
}

}  // namespace anchor::test
