#include "TestSupport.hh"
#include "adapters/backup/InMemoryBackupRepo.hh"
#include "adapters/executors/InlineExecutor.hh"
#include "core/Error.hh"
#include "core/services/BackupOrchestrator.hh"

namespace anchor::test {
using namespace kc;
using namespace testing;

TEST(BackupOrchestrator, RejectedEnqueuePreservesPreviousResultOrEmptyStatus) {
    InMemoryBackupRepo backups;
    StrictMock<MockExecutor> executor;
    StrictMock<MockFileSystem> fs;
    StrictMock<MockSnapshotter> snapshotter;
    StrictMock<MockSnapshotStore> store;
    auto backup = plan();
    Path workspace{"/workspace"};
    backups.put(Backup{backup});
    BackupOrchestrator orchestrator{backups, executor};
    orchestrator.stop();
    auto rejected = orchestrator.enqueueBackup(
        {backup, backups, snapshotter, store, fs, workspace}
    );
    EXPECT_FALSE(rejected);
    EXPECT_FALSE(backups.getRunStatus("daily"));
    backups.updateRunStatus(
        "daily",
        BackupRunStatus{
            .state = BackupRunState::failed,
            .error = Error{kc::ErrorCode::fileSystemError, "disk full"},
            .uuid = "previous"
        }
    );
    rejected = orchestrator.enqueueBackup(
        {backup, backups, snapshotter, store, fs, workspace}
    );
    EXPECT_FALSE(rejected);
    const auto status = backups.getRunStatus("daily");
    ASSERT_TRUE(status);
    EXPECT_EQ(status->state, BackupRunState::failed);
    EXPECT_EQ(status->uuid, "previous");
    ASSERT_TRUE(status->error);
    EXPECT_EQ(status->error->message(), "disk full");
}

TEST(BackupOrchestrator, CloseDrainsQueuedRunsInOrderAndRetainsResults) {
    InMemoryBackupRepo backups;
    InlineExecutor executor;
    StrictMock<MockFileSystem> fs;
    StrictMock<MockSnapshotter> snapshotter;
    StrictMock<MockSnapshotStore> store;
    auto first = plan();
    auto second = plan();
    second.name = "second";
    Path workspace{"/workspace"};
    backups.put(Backup{first});
    backups.put(Backup{second});
    BackupOrchestrator orchestrator{backups, executor};
    auto firstRun = orchestrator.enqueueBackup(
        {first, backups, snapshotter, store, fs, workspace}
    );
    auto secondRun = orchestrator.enqueueBackup(
        {second, backups, snapshotter, store, fs, workspace}
    );
    ASSERT_TRUE(firstRun);
    ASSERT_TRUE(secondRun);
    EXPECT_EQ(backups.getRunStatus("daily")->state, BackupRunState::queued);
    EXPECT_EQ(backups.getRunStatus("second")->state, BackupRunState::queued);
    InSequence sequence;
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([&](const Path& path) -> Result<void> {
            EXPECT_EQ(path.filename(), *firstRun);
            EXPECT_EQ(
                backups.getRunStatus("second")->state, BackupRunState::queued
            );
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "first failed"
            );
        }));
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([&](const Path& path) -> Result<void> {
            EXPECT_EQ(path.filename(), *secondRun);
            EXPECT_EQ(
                backups.getRunStatus("daily")->state, BackupRunState::failed
            );
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "second failed"
            );
        }));
    orchestrator.stop();
    orchestrator.start();
    orchestrator.join();
    EXPECT_EQ(backups.getRunStatus("daily")->state, BackupRunState::failed);
    EXPECT_EQ(backups.getRunStatus("second")->state, BackupRunState::failed);
}

}  // namespace anchor::test
