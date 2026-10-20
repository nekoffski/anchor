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

TEST_F(BackupServiceTest, MarksRunStartingBeforeSchedulingWork) {
    addSnapshotter();
    addStore();
    MoveOnlyFunction<void()> work;
    EXPECT_CALL(executor, call(_)).WillOnce(Invoke([&](auto callback) {
        auto status = backups.getRunStatus("daily");
        ASSERT_TRUE(status);
        EXPECT_EQ(status->state, BackupRunState::starting);
        work = std::move(callback);
    }));
    auto result = service->triggerBackup("daily");
    ASSERT_TRUE(result);
    EXPECT_TRUE(uuids::uuid::is_valid_uuid(*result));
    ASSERT_TRUE(backups.getRunStatus("daily"));
    EXPECT_EQ(backups.getRunStatus("daily")->uuid, *result);
    ASSERT_TRUE(work);
    auto duplicate = service->triggerBackup("daily");
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error().code(), ErrorCode::alreadyRunning);
}

TEST_F(
    BackupServiceTest, MarksRunStartingBeforeSchedulingAndCompletesDeferredWork
) {
    auto* snapshotter = addSnapshotter();
    auto* store = addStore();
    MoveOnlyFunction<void()> work;
    EXPECT_CALL(executor, call(_)).WillOnce(Invoke([&](auto callback) {
        auto status = backups.getRunStatus("daily");
        ASSERT_TRUE(status);
        EXPECT_EQ(status->state, BackupRunState::starting);
        work = std::move(callback);
    }));
    auto result = service->triggerBackup("daily");
    ASSERT_TRUE(result);
    EXPECT_TRUE(uuids::uuid::is_valid_uuid(*result));
    ASSERT_TRUE(backups.getRunStatus("daily"));
    EXPECT_EQ(backups.getRunStatus("daily")->uuid, *result);
    ASSERT_TRUE(work);
    auto duplicate = service->triggerBackup("daily");
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error().code(), ErrorCode::alreadyRunning);
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([&](const Path& path) -> Result<void> {
            EXPECT_EQ(path.parent().str(), "/workspace");
            EXPECT_EQ(path.filename(), *result);
            return {};
        }));
    EXPECT_CALL(fs, glob("/source/file.txt"))
        .WillOnce(Return(std::vector<Path>{"/source/file.txt"}));
    EXPECT_CALL(fs, isDirectory(_)).WillOnce(Return(false));
    EXPECT_CALL(*snapshotter, snapshot(_))
        .WillOnce(
            Invoke([&](const SnapshotArgs& args) -> Result<SnapshotMetadata> {
                EXPECT_THAT(args.name, StartsWith("daily_"));
                EXPECT_EQ(
                    backups.getRunStatus("daily")->state,
                    BackupRunState::snapshotting
                );
                return SnapshotMetadata{
                    "snapshot", Location::local("/snapshot")
                };
            })
        );
    EXPECT_CALL(*store, put(_, _))
        .WillOnce(Invoke(
            [&](const SnapshotMetadata& metadata,
                const Location&) -> Result<SnapshotMetadata> {
                EXPECT_EQ(
                    backups.getRunStatus("daily")->state,
                    BackupRunState::uploading
                );
                return metadata;
            }
        ));
    EXPECT_CALL(fs, removeAll(_)).WillOnce(Return(Result<void>{}));
    work();
    EXPECT_EQ(backups.getRunStatus("daily")->state, BackupRunState::completed);
    EXPECT_EQ(backups.getRunStatus("daily")->uuid, *result);
    service->tick();
    EXPECT_FALSE(backups.getRunStatus("daily"));
    const auto stored = backups.get("daily");
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->plan.name, backup.name);
    EXPECT_EQ(stored->plan.destination.str(), backup.destination.str());
    EXPECT_CALL(executor, call(_));
    auto next = service->triggerBackup("daily");
    ASSERT_TRUE(next);
    EXPECT_NE(*next, *result);
    EXPECT_EQ(backups.getRunStatus("daily")->uuid, *next);
}

TEST_F(BackupServiceTest, TickClearsOnlyCompletedRunsAndKeepsBackups) {
    for (const auto& [name, state] :
         std::vector<std::pair<Str, BackupRunState>>{
             {"completed", BackupRunState::completed},
             {"starting", BackupRunState::starting},
             {"snapshotting", BackupRunState::snapshotting},
             {"uploading", BackupRunState::uploading},
             {"failed", BackupRunState::failed}
         }) {
        Backup backup{plan()};
        backup.plan.name = name;
        backup.run = BackupRunStatus{.state = state};
        backups.put(backup);
    }
    service->tick();
    EXPECT_TRUE(backups.get("completed"));
    EXPECT_FALSE(backups.getRunStatus("completed"));
    EXPECT_TRUE(backups.get("daily"));
    EXPECT_FALSE(backups.getRunStatus("daily"));
    for (const auto* name :
         {"starting", "snapshotting", "uploading", "failed"}) {
        ASSERT_TRUE(backups.get(name));
        EXPECT_TRUE(backups.getRunStatus(name));
    }
    service->tick();
    EXPECT_TRUE(backups.getRunStatus("failed"));
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
