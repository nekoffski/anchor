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
        sources:
          - root: /one
            includes: ["", "*.txt"]
          - root: $ANCHOR_TEST_PATH/two
            includes: []
    )"));
    const std::vector<Str> invalid{
        "settings: [unterminated",
        "{}",
        "settings: null\nsources: []",
        "settings: scalar\nsources: []",
        "settings: [directory]\nsources: []",
        "settings: {archive: directory}\nsources: []",
        "settings: {destination: ftp://bad, archive: directory}\nsources: []",
        "settings: {destination: local:///backup}\nsources: []",
        "settings: {destination: local:///backup, archive: rar}\nsources: []",
        "settings: {destination: local:///backup, archive: "
        "directory}\nsources: bad",
        "settings: {destination: local:///backup, archive: directory}",
        "settings: {destination: local:///backup, archive: "
        "directory}\nsources: [valid, {}]",
        "settings: {destination: local:///backup, archive: "
        "directory}\nsources: [null]",
        "settings: {destination: null, archive: directory}\nsources: []",
        "settings: {destination: local:///backup, archive: null}\nsources: []",
    };
    for (size_t i = 0; i < invalid.size(); ++i) {
        ASSERT_TRUE(
            fs.writeFile(path(fmt::format("invalid{}.yaml", i)), invalid[i])
        );
    }
    ScopedEnv env{"ANCHOR_TEST_PATH", "1337"};
    OnDiskBackupPlanRepo repo{root, fs};
    std::vector<BackupPlan> plans;
    auto result =
        repo.forEach([&](const BackupPlan& plan) { plans.push_back(plan); });
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), HasSubstr("Could not load plan"));
    ASSERT_EQ(plans.size(), 1);
    const auto& plan = plans.front();
    EXPECT_EQ(plan.name, "daily");
    EXPECT_EQ(plan.archiveType, ArchiveType::directory);
    EXPECT_EQ(plan.destination.str(), "local:///backup");
    ASSERT_EQ(plan.sources.size(), 2);
    EXPECT_EQ(plan.sources[0].root.str(), "/one");
    EXPECT_EQ(plan.sources[1].root.str(), "1337/two");
    EXPECT_THAT(plan.sources[0].includes, ElementsAre("", "*.txt"));
    EXPECT_TRUE(plan.sources[1].includes.empty());
}

TEST_F(ConfigTest, PlanRepoParsesYmlPlansWithEmptySources) {
    ASSERT_TRUE(fs.writeFile(path("empty.yml"), R"(
        settings: {destination: local:///backup, archive: directory}
        sources: []
    )"));
    OnDiskBackupPlanRepo repo{root, fs};
    std::vector<BackupPlan> plans;
    repo.forEach([&](const BackupPlan& plan) { plans.push_back(plan); });
    ASSERT_EQ(plans.size(), 1);
    EXPECT_EQ(plans.front().name, "empty");
    EXPECT_TRUE(plans.front().sources.empty());
}

TEST_F(ConfigTest, PlanRepoParsesOptionalCronAndRejectsInvalidSchedules) {
    OnDiskBackupPlanRepo repo{root, fs};
    auto write = [&](const Str& cron) {
        ASSERT_TRUE(fs.writeFile(
            path("scheduled.yaml"),
            "settings:\n  destination: local:///backup\n  archive: "
            "directory\n" +
                cron + "sources: []\n"
        ));
    };
    write("  cron: '0 */5 * * * *'\n");
    std::vector<BackupPlan> plans;
    repo.forEach([&](const BackupPlan& plan) { plans.push_back(plan); });
    ASSERT_EQ(plans.size(), 1);
    ASSERT_TRUE(plans.front().cron);
    EXPECT_TRUE(plans.front().cron->matches(
        std::chrono::sys_seconds{std::chrono::seconds{1'704'067'200}}
    ));
    write("");
    plans.clear();
    repo.forEach([&](const BackupPlan& plan) { plans.push_back(plan); });
    ASSERT_EQ(plans.size(), 1);
    EXPECT_FALSE(plans.front().cron);
    for (const auto* invalid :
         {"null", "[]", "{}", "''", "'invalid'", "'* * * * *'",
          "'60 * * * * *'"}) {
        SCOPED_TRACE(invalid);
        write(Str{"  cron: "} + invalid + "\n");
        repo.forEach([](const BackupPlan&) {
            FAIL() << "invalid cron must reject the plan";
        });
    }
}

TEST_F(ConfigTest, PlanRepoRejectsMalformedSourcesAndIncludes) {
    const std::vector<Str> invalid{
        "sources: null",
        "sources: {}",
        "sources: [/one]",
        "sources: [null]",
        "sources: [{}]",
        "sources: [{root: null, includes: []}]",
        "sources: [{root: [], includes: []}]",
        "sources: [{root: {}, includes: []}]",
        "sources: [{root: /one}]",
        "sources: [{root: /one, includes: null}]",
        "sources: [{root: /one, includes: text}]",
        "sources: [{root: /one, includes: {}}]",
        "sources: [{root: /one, includes: [valid, null]}]",
        "sources: [{root: /one, includes: [valid, {}]}]",
        "sources: [{root: /one, includes: [valid, []]}]",
        "sources: [{root: /one, includes: []}, {}]",
    };
    OnDiskBackupPlanRepo repo{root, fs};
    for (const auto& sources : invalid) {
        SCOPED_TRACE(sources);
        ASSERT_TRUE(fs.writeFile(
            path("invalid.yaml"),
            "settings: {destination: local:///backup, archive: directory}\n" +
                sources
        ));
        repo.forEach([](const BackupPlan&) {
            FAIL() << "invalid sources must reject the whole plan";
        });
    }
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
    auto iteration = repo.forEach([](const BackupPlan&) {
        FAIL() << "no plans on listing failure";
    });
    ASSERT_FALSE(iteration);
    EXPECT_EQ(iteration.error().message(), "listing failed");
}

}  // namespace anchor::test
