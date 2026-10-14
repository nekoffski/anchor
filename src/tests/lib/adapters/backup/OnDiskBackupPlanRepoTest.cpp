#include "TestSupport.hh"
#include "adapters/backup/OnDiskBackupPlanRepo.hh"
#include "base/Config.hh"
#include "base/LibconfigReader.hh"

namespace anchor::test {
using namespace testing;
using ConfigTest = TemporaryDirectory;

TEST_F(ConfigTest, PlanRepoValidatesDirectoryAndFiltersConfFiles) {
    OnDiskBackupPlanRepo repo{root, fs};
    EXPECT_TRUE(repo.valid());
    ASSERT_TRUE(fs.touch(path("daily.conf")));
    ASSERT_TRUE(fs.touch(path("notes.txt")));
    ASSERT_TRUE(fs.touch(path("weekly.CONF")));
    ASSERT_TRUE(fs.createDirectory(path("directory.conf")));
    auto paths = repo.list();
    ASSERT_TRUE(paths);
    ASSERT_EQ(paths->size(), 1);
    EXPECT_EQ((*paths)[0].filenameWithExtension(), "daily.conf");
    OnDiskBackupPlanRepo missing{path("absent"), fs};
    auto result = missing.valid();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
}

TEST_F(ConfigTest, PlanRepoParsesPlansAndSkipsReportedErrors) {
    ASSERT_TRUE(fs.writeFile(path("daily.conf"), R"(
        settings = { destination = "local:///backup"; archive = "directory"; };
        filters = ("/one", "$ANCHOR_TEST_PATH/two");
    )"));
    const std::vector<Str> invalid{
        "not valid {{",
        "settings={archive=\"directory\";}; filters=();",
        "settings={destination=\"ftp://bad\"; archive=\"directory\";}; "
        "filters=();",
        "settings={destination=\"local:///backup\";}; filters=();",
        "settings={destination=\"local:///backup\"; archive=\"rar\";}; "
        "filters=();",
        "settings={destination=\"local:///backup\"; archive=\"directory\";}; "
        "filters=\"bad\";",
        "settings={destination=\"local:///backup\"; archive=\"directory\";};",
        "settings={destination=\"local:///backup\"; archive=\"directory\";}; "
        "filters=(\"valid\",2);",
    };
    for (size_t i = 0; i < invalid.size(); ++i) {
        ASSERT_TRUE(
            fs.writeFile(path(fmt::format("invalid{}.conf", i)), invalid[i])
        );
    }
    OnDiskBackupPlanRepo repo{root, fs};
    std::vector<BackupPlan> plans;
    repo.forEach([&](const BackupPlan& plan) { plans.push_back(plan); });
    ASSERT_EQ(plans.size(), 1);
    const auto& plan = plans.front();
    EXPECT_EQ(plan.name, "daily");
    EXPECT_EQ(plan.archiveType, ArchiveType::directory);
    EXPECT_EQ(plan.destination.str(), "local:///backup");
    ASSERT_EQ(plan.filters.size(), 2);
    EXPECT_EQ(plan.filters[0].str(), "/one");
    EXPECT_EQ(plan.filters[1].str(), "$ANCHOR_TEST_PATH/two");
}

TEST_F(ConfigTest, PlanRepoPropagatesListingErrorsAndSkipsIteration) {
    StrictMock<MockFileSystem> mock;
    OnDiskBackupPlanRepo repo{root, mock};
    EXPECT_CALL(mock, listFiles(_))
        .Times(2)
        .WillRepeatedly(Return(
            Error::unexpected(ErrorCode::fileSystemError, "listing failed")
        ));
    auto result = repo.list();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().message(), "listing failed");
    repo.forEach([](const BackupPlan&) {
        FAIL() << "no plans on listing failure";
    });
}

}  // namespace anchor::test
