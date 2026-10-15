#include "TestSupport.hh"
#include "adapters/backup/OnDiskBackupPlanRepo.hh"

namespace anchor::test {
using namespace testing;
using ConfigTest = TemporaryDirectory;

TEST_F(ConfigTest, PlanRepoValidatesDirectoryAndFiltersYamlFiles) {
    OnDiskBackupPlanRepo repo{root, fs};
    EXPECT_TRUE(repo.valid());
    ASSERT_TRUE(fs.touch(path("daily.yaml")));
    ASSERT_TRUE(fs.touch(path("weekly.yml")));
    ASSERT_TRUE(fs.touch(path("notes.txt")));
    ASSERT_TRUE(fs.touch(path("weekly.YAML")));
    ASSERT_TRUE(fs.createDirectory(path("directory.yaml")));
    auto paths = repo.list();
    ASSERT_TRUE(paths);
    std::vector<Str> names;
    for (const auto& file : *paths) {
        names.push_back(file.filenameWithExtension());
    }
    EXPECT_THAT(names, UnorderedElementsAre("daily.yaml", "weekly.yml"));
    OnDiskBackupPlanRepo missing{path("absent"), fs};
    auto result = missing.valid();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
}

TEST_F(ConfigTest, PlanRepoParsesPlansAndSkipsReportedErrors) {
    ASSERT_TRUE(fs.writeFile(path("daily.yaml"), R"(
        settings:
          destination: local:///backup
          archive: directory
        filters:
          - /one
          - $ANCHOR_TEST_PATH/two
    )"));
    const std::vector<Str> invalid{
        "settings: [unterminated",
        "settings: {archive: directory}\nfilters: []",
        "settings: {destination: ftp://bad, archive: directory}\nfilters: []",
        "settings: {destination: local:///backup}\nfilters: []",
        "settings: {destination: local:///backup, archive: rar}\nfilters: []",
        "settings: {destination: local:///backup, archive: "
        "directory}\nfilters: bad",
        "settings: {destination: local:///backup, archive: directory}",
        "settings: {destination: local:///backup, archive: "
        "directory}\nfilters: [valid, {}]",
        "settings: {destination: local:///backup, archive: "
        "directory}\nfilters: [null]",
    };
    for (size_t i = 0; i < invalid.size(); ++i) {
        ASSERT_TRUE(
            fs.writeFile(path(fmt::format("invalid{}.yaml", i)), invalid[i])
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
