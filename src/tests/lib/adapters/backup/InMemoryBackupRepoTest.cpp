#include "TestSupport.hh"
#include "adapters/backup/InMemoryBackupRepo.hh"

namespace anchor::test {
using namespace testing;

TEST(InMemoryBackupRepo, StoresReplacesAndReturnsIndependentCopies) {
    InMemoryBackupRepo repo;
    EXPECT_FALSE(repo.get("daily"));
    Backup backup{
        .plan = plan(),
        .enabled = false,
        .run = BackupRunStatus{
            .state = BackupRunState::starting, .uuid = "first-run"
        }
    };
    repo.put(backup);
    auto copy = repo.get("daily");
    ASSERT_TRUE(copy);
    EXPECT_FALSE(copy->enabled);
    ASSERT_TRUE(copy->run);
    EXPECT_EQ(copy->run->uuid, "first-run");
    copy->plan.sources.front().includes.clear();
    copy->run->state = BackupRunState::completed;
    copy->enabled = true;
    auto stored = repo.get("daily");
    ASSERT_TRUE(stored);
    EXPECT_FALSE(stored->enabled);
    EXPECT_THAT(stored->plan.sources.front().includes, ElementsAre("file.txt"));
    EXPECT_EQ(stored->run->state, BackupRunState::starting);
    repo.put(*copy);
    auto replaced = repo.get("daily");
    ASSERT_TRUE(replaced);
    EXPECT_TRUE(replaced->enabled);
    EXPECT_TRUE(replaced->plan.sources.front().includes.empty());
    EXPECT_EQ(replaced->run->state, BackupRunState::completed);
}

TEST(InMemoryBackupRepo, IteratesFullBackups) {
    InMemoryBackupRepo repo;
    repo.forEach([](const Backup&) { FAIL() << "repo should be empty"; });
    Backup first{plan()};
    first.plan.name = "first";
    repo.put(first);
    Backup second{
        .plan = plan(),
        .enabled = false,
        .run = BackupRunStatus{
            .state = BackupRunState::completed, .uuid = "run-id"
        }
    };
    second.plan.name = "second";
    repo.put(second);
    std::vector<Str> names;
    repo.forEach([&](const Backup& backup) {
        names.push_back(backup.plan.name);
        if (backup.plan.name == "second") {
            EXPECT_FALSE(backup.enabled);
            ASSERT_TRUE(backup.run);
            EXPECT_EQ(backup.run->state, BackupRunState::completed);
            EXPECT_EQ(backup.run->uuid, "run-id");
        } else {
            EXPECT_TRUE(backup.enabled);
            EXPECT_FALSE(backup.run);
        }
    });
    EXPECT_THAT(names, UnorderedElementsAre("first", "second"));
}

TEST(InMemoryBackupRepo, UpdatesAndClearsRunStatusWithoutChangingBackup) {
    InMemoryBackupRepo repo;
    Backup backup{.plan = plan(), .enabled = false};
    repo.put(backup);
    EXPECT_FALSE(repo.getRunStatus("daily"));
    repo.updateRunStatus(
        "daily", BackupRunStatus{
                     .state = BackupRunState::failed,
                     .error = Error{ErrorCode::fileSystemError, "disk full"},
                     .uuid = "run-id"
                 }
    );
    auto failed = repo.getRunStatus("daily");
    ASSERT_TRUE(failed);
    EXPECT_EQ(failed->state, BackupRunState::failed);
    EXPECT_EQ(failed->uuid, "run-id");
    ASSERT_TRUE(failed->error);
    EXPECT_EQ(failed->error->code(), ErrorCode::fileSystemError);
    EXPECT_EQ(failed->error->message(), "disk full");
    failed->error.reset();
    EXPECT_TRUE(repo.getRunStatus("daily")->error);
    repo.updateRunStatus(
        "daily",
        BackupRunStatus{.state = BackupRunState::completed, .uuid = "run-id"}
    );
    EXPECT_EQ(repo.getRunStatus("daily")->state, BackupRunState::completed);
    EXPECT_FALSE(repo.getRunStatus("daily")->error);
    repo.clearRunStatus("daily");
    EXPECT_FALSE(repo.getRunStatus("daily"));
    const auto stored = repo.get("daily");
    ASSERT_TRUE(stored);
    EXPECT_FALSE(stored->run);
    EXPECT_FALSE(stored->enabled);
    EXPECT_EQ(stored->plan.name, backup.plan.name);
    EXPECT_EQ(stored->plan.destination.str(), backup.plan.destination.str());
    EXPECT_EQ(stored->plan.archiveType, backup.plan.archiveType);
    ASSERT_EQ(stored->plan.sources.size(), 1);
    EXPECT_EQ(stored->plan.sources.front().root.str(), "/source");
    EXPECT_THAT(stored->plan.sources.front().includes, ElementsAre("file.txt"));
}

TEST(InMemoryBackupRepo, RunStatusHelpersDoNotCreateMissingBackups) {
    InMemoryBackupRepo repo;
    EXPECT_FALSE(repo.getRunStatus("missing"));
    repo.updateRunStatus(
        "missing", BackupRunStatus{.state = BackupRunState::starting}
    );
    EXPECT_FALSE(repo.get("missing"));
    repo.clearRunStatus("missing");
    EXPECT_FALSE(repo.get("missing"));
}

}  // namespace anchor::test
