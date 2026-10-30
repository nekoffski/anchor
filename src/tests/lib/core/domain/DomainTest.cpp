#include "LogCapture.hh"
#include "TestSupport.hh"
#include "core/Error.hh"
#include "core/domain/Backup.hh"
#include "core/domain/Cron.hh"
#include "core/domain/Location.hh"

namespace anchor::test {
using namespace kc;
using namespace testing;
using namespace std::chrono;
using namespace std::chrono_literals;
using CronLogTest = LogCapture;

TEST_F(CronLogTest, EvaluationFailuresLogTheExpressionAndReason) {
    KCORE_DEFER { tzset(); };
    ScopedEnv timezone{"TZ", "UTC0"};
    tzset();
    log::setLogLevel(log::Level::warn);
    auto expression = Cron::fromString("* * * * * *");
    ASSERT_TRUE(expression);
    // croncpp treats the second before the Unix epoch as an invalid time.
    EXPECT_FALSE(expression->matches(sys_seconds{-1s}));
    EXPECT_THAT(
        output.str(),
        HasSubstr("Could not evaluate cron expression '* * * * * *'")
    );
    EXPECT_THAT(output.str(), HasSubstr("Invalid time expression"));
}

TEST(ArchiveType, RoundTripsSupportedTypesAndRejectsUnknownValues) {
    for (auto type :
         {ArchiveType::directory, ArchiveType::zip, ArchiveType::tgz}) {
        auto parsed = archiveTypeFromStr(archiveTypeToStr(type));
        ASSERT_TRUE(parsed);
        EXPECT_EQ(*parsed, type);
    }
    for (const auto* value : {"", "ZIP", "tar", "unknown"}) {
        auto result = archiveTypeFromStr(value);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
    }
}

TEST(BackupRunState, FormatsEveryState) {
    EXPECT_EQ(backupRunStateToString(BackupRunState::queued), "queued");
    EXPECT_EQ(backupRunStateToString(BackupRunState::starting), "starting");
    EXPECT_EQ(
        backupRunStateToString(BackupRunState::snapshotting), "snapshotting"
    );
    EXPECT_EQ(backupRunStateToString(BackupRunState::uploading), "uploading");
    EXPECT_EQ(backupRunStateToString(BackupRunState::completed), "completed");
    EXPECT_EQ(backupRunStateToString(BackupRunState::failed), "failed");
}

TEST(BackupRunState, OnlyNonterminalStatesAreActive) {
    for (auto state :
         {BackupRunState::queued, BackupRunState::starting,
          BackupRunState::snapshotting, BackupRunState::uploading}) {
        EXPECT_TRUE(isActive(state));
    }
    EXPECT_FALSE(isActive(BackupRunState::completed));
    EXPECT_FALSE(isActive(BackupRunState::failed));
}

TEST(Cron, ParsesSixFieldExpressionsAndRejectsInvalidInput) {
    EXPECT_TRUE(Cron::fromString("0 */5 * * * *"));
    for (const auto* expression :
         {"", "invalid", "* * * * *", "60 * * * * *"}) {
        auto result = Cron::fromString(expression);
        ASSERT_FALSE(result) << expression;
        EXPECT_EQ(result.error().code(), kc::ErrorCode::invalidArgument);
    }
}

TEST(Cron, MatchesOccurrencesInTheMachineTimezone) {
    KCORE_DEFER { tzset(); };
    ScopedEnv timezone{"TZ", "UTC-2"};
    tzset();
    const sys_seconds midnightUtc = sys_days{2024y / January / 1};
    auto expression = Cron::fromString("0 0 3 * * *");
    ASSERT_TRUE(expression);
    EXPECT_FALSE(expression->matches(midnightUtc));
    EXPECT_TRUE(expression->matches(midnightUtc + 1h));
    EXPECT_FALSE(expression->matches(midnightUtc + 1h + 1s));
    EXPECT_TRUE(expression->matches(midnightUtc + 1h + 24h));
}

TEST(Cron, ImpossibleCalendarDateDoesNotMatch) {
    auto expression = Cron::fromString("0 0 0 31 2 *");
    ASSERT_TRUE(expression);
    EXPECT_FALSE(expression->matches(sys_days{2024y / January / 1}));
    EXPECT_FALSE(expression->matches(sys_seconds::min()));
}

TEST(Cron, NextDeadlineSkipsMissingLocalTimesAndKeepsRepeatedOnes) {
    KCORE_DEFER { tzset(); };
    ScopedEnv timezone{"TZ", "Europe/Warsaw"};
    tzset();
    auto expression = Cron::fromString("0 0 2 * * *");
    ASSERT_TRUE(expression);
    const sys_seconds beforeSpring = sys_days{2024y / March / 30} + 2h;
    const sys_seconds afterSpring = sys_days{2024y / April / 1};
    EXPECT_EQ(expression->nextAfter(beforeSpring), afterSpring);
    const sys_seconds firstAutumn = sys_days{2024y / October / 27};
    EXPECT_EQ(expression->nextAfter(firstAutumn - 1s), firstAutumn);
    EXPECT_EQ(expression->nextAfter(firstAutumn), firstAutumn + 1h);
}

TEST(Cron, NextDeadlinePreservesLaterTimesOnTheSpringTransitionDay) {
    KCORE_DEFER { tzset(); };
    ScopedEnv timezone{"TZ", "Europe/Warsaw"};
    tzset();
    auto expression = Cron::fromString("0 0 2,4 * * *");
    ASSERT_TRUE(expression);
    const sys_seconds beforeSpring = sys_days{2024y / March / 30} + 4h;
    const sys_seconds afterSpring = sys_days{2024y / March / 31} + 2h;
    EXPECT_EQ(expression->nextAfter(beforeSpring), afterSpring);
    EXPECT_EQ(expression->nextAfter(afterSpring), sys_days{2024y / April / 1});
}

TEST(Cron, NextDeadlineIsStrictlyFutureAndImpossibleDatesHaveNoDeadline) {
    KCORE_DEFER { tzset(); };
    ScopedEnv timezone{"TZ", "UTC0"};
    tzset();
    auto daily = Cron::fromString("0 0 0 * * *");
    auto everySecond = Cron::fromString("* * * * * *");
    auto impossible = Cron::fromString("0 0 0 31 2 *");
    ASSERT_TRUE(daily);
    ASSERT_TRUE(everySecond);
    ASSERT_TRUE(impossible);
    const sys_seconds midnight = sys_days{2024y / January / 1};
    EXPECT_EQ(daily->nextAfter(midnight - 1s), midnight);
    EXPECT_EQ(daily->nextAfter(midnight), midnight + 24h);
    EXPECT_EQ(everySecond->nextAfter(midnight), midnight + 1s);
    EXPECT_FALSE(impossible->nextAfter(midnight));
}

TEST(Cron, MatchesActualLocalTimesAcrossDaylightSavingChanges) {
    KCORE_DEFER { tzset(); };
    ScopedEnv timezone{"TZ", "Europe/Warsaw"};
    tzset();
    auto expression = Cron::fromString("0 0 2 * * *");
    ASSERT_TRUE(expression);
    const sys_seconds beforeSpringChange =
        sys_days{2024y / March / 31} + 59min + 59s;
    EXPECT_FALSE(expression->matches(beforeSpringChange + 1s));
    EXPECT_TRUE(expression->matches(beforeSpringChange + 1s + 23h));
    const sys_seconds firstOccurrence = sys_days{2024y / October / 27};
    const auto secondOccurrence = firstOccurrence + 1h;
    EXPECT_TRUE(expression->matches(firstOccurrence));
    EXPECT_TRUE(expression->matches(secondOccurrence));
}

TEST(Location, ParsesAndFormatsLocalAndS3Locations) {
    for (const auto& location :
         {Location::local("/tmp/backup"), Location::s3("bucket/key")}) {
        auto parsed = Location::fromStr(location.str());
        ASSERT_TRUE(parsed);
        EXPECT_EQ(parsed->type, location.type);
        EXPECT_EQ(parsed->path, location.path);
        EXPECT_EQ(
            *locationTypeFromStr(locationTypeToStr(location.type)),
            location.type
        );
    }
    EXPECT_EQ(Location::local("/tmp").str(), "local:///tmp");
    EXPECT_EQ(Location::s3("bucket/key").str(), "s3://bucket/key");
}

TEST(Location, RejectsMalformedAndUnknownSchemes) {
    for (const auto* text :
         {"", "/tmp", "local:/tmp", "local://one://two", "ftp://path",
          "://path"}) {
        auto location = Location::fromStr(text);
        ASSERT_FALSE(location) << text;
        EXPECT_EQ(location.error().code(), kc::ErrorCode::invalidArgument);
    }
    auto type = locationTypeFromStr("LOCAL");
    ASSERT_FALSE(type);
    EXPECT_EQ(type.error().code(), kc::ErrorCode::invalidArgument);
}

}  // namespace anchor::test
