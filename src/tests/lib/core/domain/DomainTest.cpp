#include "TestSupport.hh"
#include "core/domain/Backup.hh"
#include "core/domain/Cron.hh"
#include "core/domain/Location.hh"

namespace anchor::test {
using namespace testing;

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
        EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
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

TEST(Cron, ParsesSixFieldExpressionsAndRejectsInvalidInput) {
    EXPECT_TRUE(Cron::fromString("0 */5 * * * *"));
    for (const auto* expression :
         {"", "invalid", "* * * * *", "60 * * * * *"}) {
        auto result = Cron::fromString(expression);
        ASSERT_FALSE(result) << expression;
        EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
    }
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
        EXPECT_EQ(location.error().code(), ErrorCode::invalidArgument);
    }
    auto type = locationTypeFromStr("LOCAL");
    ASSERT_FALSE(type);
    EXPECT_EQ(type.error().code(), ErrorCode::invalidArgument);
}

}  // namespace anchor::test
