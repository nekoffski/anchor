#include <future>

#include "TestSupport.hh"
#include "adapters/backup/InMemoryBackupRepo.hh"
#include "adapters/executors/InlineExecutor.hh"
#include "core/services/BackupService.hh"

namespace anchor::test {
using namespace testing;

class BackupServiceTest : public Test {
   protected:
    StrictMock<MockFileSystem> fs;
    InMemoryBackupRepo backups;
    StrictMock<MockPlanRepo> plans;
    StrictMock<MockExecutor> executor;
    Snapshotters snapshotters;
    SnapshotStores stores;
    Config config;
    BackupPlan backup = plan();
    std::unique_ptr<BackupService> service;

    void SetUp() override {
        config.paths.workspace = "/workspace";
        EXPECT_CALL(plans, forEach(_)).WillOnce(Invoke([&](auto callback) {
            callback(backup);
        }));
        service = std::make_unique<BackupService>(
            config, fs, Ports{backups, plans, snapshotters, stores, executor}
        );
    }
    MockSnapshotter* addSnapshotter() {
        auto owned = std::make_unique<StrictMock<MockSnapshotter>>();
        auto* pointer = owned.get();
        snapshotters.add(ArchiveType::directory, std::move(owned));
        return pointer;
    }
    MockSnapshotStore* addStore() {
        auto owned = std::make_unique<StrictMock<MockSnapshotStore>>();
        auto* pointer = owned.get();
        stores.add(LocationType::local, std::move(owned));
        return pointer;
    }
};

TEST_F(BackupServiceTest, UnknownPlanDoesNotCreateRunOrScheduleWork) {
    auto result = service->triggerBackup("missing");
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
    EXPECT_EQ(result.error().message(), "Backup plan not found");
    EXPECT_FALSE(backups.get("missing"));
}

TEST_F(BackupServiceTest, RepeatedShutdownJoinsWorkersBeforeDestruction) {
    for (int i = 0; i < 20; ++i) {
        service.reset();
        EXPECT_CALL(plans, forEach(_));
        service = std::make_unique<BackupService>(
            config, fs, Ports{backups, plans, snapshotters, stores, executor}
        );
    }
    service.reset();
}

TEST_F(BackupServiceTest, LoadingPlansPreservesStoredBackupState) {
    backups.put(
        Backup{
            .plan = backup,
            .enabled = false,
            .run = BackupRunStatus{
                .state = BackupRunState::uploading, .uuid = "existing-run"
            }
        }
    );
    backup.destination = Location::local("/new-backups");
    EXPECT_CALL(plans, forEach(_)).WillOnce(Invoke([&](auto callback) {
        callback(backup);
    }));
    service = std::make_unique<BackupService>(
        config, fs, Ports{backups, plans, snapshotters, stores, executor}
    );
    const auto stored = backups.get("daily");
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->plan.destination.str(), "local:///new-backups");
    EXPECT_FALSE(stored->enabled);
    ASSERT_TRUE(stored->run);
    EXPECT_EQ(stored->run->state, BackupRunState::uploading);
    EXPECT_EQ(stored->run->uuid, "existing-run");
}

TEST_F(BackupServiceTest, ExistingRunDoesNotScheduleDuplicateWork) {
    backups.updateRunStatus(
        "daily", BackupRunStatus{
                     .state = BackupRunState::uploading, .uuid = "existing-run"
                 }
    );
    auto result = service->triggerBackup("daily");
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::alreadyRunning);
    EXPECT_EQ(backups.getRunStatus("daily")->state, BackupRunState::uploading);
    EXPECT_EQ(backups.getRunStatus("daily")->uuid, "existing-run");
}

TEST_F(BackupServiceTest, MissingSnapshotterDoesNotCreateRun) {
    addStore();
    auto result = service->triggerBackup("daily");
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
    EXPECT_EQ(result.error().message(), "Snapshotter not found");
    EXPECT_FALSE(backups.getRunStatus("daily"));
    EXPECT_TRUE(backups.get("daily"));
}

TEST_F(BackupServiceTest, MissingStoreDoesNotCreateRun) {
    addSnapshotter();
    auto result = service->triggerBackup("daily");
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
    EXPECT_EQ(result.error().message(), "Snapshot store not found");
    EXPECT_FALSE(backups.getRunStatus("daily"));
    EXPECT_TRUE(backups.get("daily"));
}

TEST_F(BackupServiceTest, MarksRunQueuedBeforeDispatchingWork) {
    addSnapshotter();
    addStore();
    std::promise<MoveOnlyFunction<void()>> submitted;
    auto work = submitted.get_future();
    EXPECT_CALL(executor, call(_)).WillOnce(Invoke([&](auto callback) {
        auto status = backups.getRunStatus("daily");
        EXPECT_TRUE(status);
        if (status) {
            EXPECT_EQ(status->state, BackupRunState::queued);
        }
        submitted.set_value(std::move(callback));
    }));
    auto result = service->triggerBackup("daily");
    auto duplicate = service->triggerBackup("daily");
    const auto workStatus = work.wait_for(std::chrono::seconds{1});
    service.reset();

    ASSERT_TRUE(result);
    EXPECT_TRUE(uuids::uuid::is_valid_uuid(*result));
    ASSERT_TRUE(backups.getRunStatus("daily"));
    EXPECT_EQ(backups.getRunStatus("daily")->uuid, *result);
    ASSERT_EQ(workStatus, std::future_status::ready);
    EXPECT_TRUE(work.get());
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error().code(), ErrorCode::alreadyRunning);
}

TEST_F(BackupServiceTest, ShutdownDrainsQueuedBackupAndCompletesWork) {
    auto* snapshotter = addSnapshotter();
    auto* store = addStore();
    EXPECT_CALL(executor, call(_)).WillOnce(Invoke([&](auto callback) {
        const auto status = backups.getRunStatus("daily");
        EXPECT_TRUE(status);
        if (status) {
            EXPECT_EQ(status->state, BackupRunState::queued);
        }
        callback();
    }));
    InSequence sequence;
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([&](const Path&) -> Result<void> {
            EXPECT_EQ(
                backups.getRunStatus("daily")->state, BackupRunState::starting
            );
            return {};
        }));
    EXPECT_CALL(fs, glob("/source/file.txt"))
        .WillOnce(Return(std::vector<Path>{"/source/file.txt"}));
    EXPECT_CALL(fs, isDirectory(_)).WillOnce(Return(false));
    SnapshotMetadata metadata{"snapshot", Location::local("/snapshot")};
    EXPECT_CALL(*snapshotter, snapshot(_)).WillOnce(Return(metadata));
    EXPECT_CALL(*store, put(_, _)).WillOnce(Return(metadata));
    EXPECT_CALL(fs, removeAll(_)).WillOnce(Return(Result<void>{}));

    auto result = service->triggerBackup("daily");
    service.reset();

    ASSERT_TRUE(result);
    const auto status = backups.getRunStatus("daily");
    ASSERT_TRUE(status);
    EXPECT_EQ(status->state, BackupRunState::completed);
    EXPECT_EQ(status->uuid, *result);
    EXPECT_TRUE(backups.get("daily"));
}

TEST(BackupOrchestrator, IdleScanClearsOnlyCompletedRuns) {
    StrictMock<MockBackupRepo> backups;
    StrictMock<MockExecutor> executor;
    std::promise<void> cleared;
    auto completed = cleared.get_future();
    EXPECT_CALL(backups, forEach(_))
        .WillOnce(Invoke([](auto callback) {
            for (auto state :
                 {BackupRunState::queued, BackupRunState::starting,
                  BackupRunState::snapshotting, BackupRunState::uploading,
                  BackupRunState::failed, BackupRunState::completed}) {
                Backup backup{plan()};
                backup.plan.name = backupRunStateToString(state);
                backup.run = BackupRunStatus{.state = state};
                callback(backup);
            }
        }))
        .WillRepeatedly(Return());
    EXPECT_CALL(backups, clearRunStatus("completed")).WillOnce(Invoke([&] {
        cleared.set_value();
    }));

    BackupOrchestrator orchestrator{backups, executor};
    orchestrator.start();
    auto status = completed.wait_for(std::chrono::seconds{1});
    orchestrator.stop();
    orchestrator.join();
    EXPECT_EQ(status, std::future_status::ready);
}

TEST(Ports, AccessorsPreserveInjectedObjects) {
    InMemoryBackupRepo backups;
    StrictMock<MockPlanRepo> plans;
    Snapshotters snapshotters;
    SnapshotStores stores;
    InlineExecutor executor;
    Ports ports{backups, plans, snapshotters, stores, executor};
    EXPECT_EQ(&ports.backupRepo(), &backups);
    EXPECT_EQ(&ports.planRepo(), &plans);
    EXPECT_EQ(&ports.snapshotters(), &snapshotters);
    EXPECT_EQ(&ports.snapshotStores(), &stores);
    EXPECT_EQ(&ports.executor(), &executor);
}

TEST(InlineExecutor, CallsMoveOnlyCallbackSynchronously) {
    InlineExecutor executor;
    int result = 0;
    executor.call([value = std::make_unique<int>(42), &result] {
        result = *value;
    });
    EXPECT_EQ(result, 42);
}

}  // namespace anchor::test
