#include "TestSupport.hh"
#include "adapters/backup/InMemoryBackupRunRepo.hh"
#include "adapters/executors/InlineExecutor.hh"
#include "core/services/BackupService.hh"

namespace anchor::test {
using namespace testing;

class BackupServiceTest : public Test {
   protected:
    StrictMock<MockFileSystem> fs;
    InMemoryBackupRunRepo runs;
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
            config, fs, Ports{runs, plans, snapshotters, stores, executor}
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
    service->triggerBackup("missing");
    EXPECT_FALSE(runs.has("missing"));
}

TEST_F(BackupServiceTest, ExistingRunDoesNotScheduleDuplicateWork) {
    runs.put("daily", {.state = BackupRunState::uploading});
    service->triggerBackup("daily");
    EXPECT_EQ(runs.get("daily")->state, BackupRunState::uploading);
}

TEST_F(BackupServiceTest, MissingSnapshotterDoesNotCreateRun) {
    addStore();
    service->triggerBackup("daily");
    EXPECT_FALSE(runs.has("daily"));
}

TEST_F(BackupServiceTest, MissingStoreDoesNotCreateRun) {
    addSnapshotter();
    service->triggerBackup("daily");
    EXPECT_FALSE(runs.has("daily"));
}

TEST_F(BackupServiceTest, MarksRunStartingBeforeSchedulingWork) {
    addSnapshotter();
    addStore();
    MoveOnlyFunction<void()> work;
    EXPECT_CALL(executor, call(_)).WillOnce(Invoke([&](auto callback) {
        auto status = runs.get("daily");
        ASSERT_TRUE(status);
        EXPECT_EQ(status->state, BackupRunState::starting);
        work = std::move(callback);
    }));
    service->triggerBackup("daily");
    ASSERT_TRUE(work);
    service->triggerBackup("daily");
}

TEST_F(
    BackupServiceTest, MarksRunStartingBeforeSchedulingAndCompletesDeferredWork
) {
    auto* snapshotter = addSnapshotter();
    auto* store = addStore();
    MoveOnlyFunction<void()> work;
    EXPECT_CALL(executor, call(_)).WillOnce(Invoke([&](auto callback) {
        auto status = runs.get("daily");
        ASSERT_TRUE(status);
        EXPECT_EQ(status->state, BackupRunState::starting);
        work = std::move(callback);
    }));
    service->triggerBackup("daily");
    ASSERT_TRUE(work);
    service->triggerBackup("daily");
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([](const Path& path) -> Result<void> {
            EXPECT_EQ(path.parent().str(), "/workspace");
            return {};
        }));
    EXPECT_CALL(fs, glob("/source/file.txt"))
        .WillOnce(Return(std::vector<Path>{"/source/file.txt"}));
    EXPECT_CALL(fs, isDirectory(_)).WillOnce(Return(false));
    EXPECT_CALL(*snapshotter, snapshot(_))
        .WillOnce(
            Invoke([&](const SnapshotArgs& args) -> Result<SnapshotMetadata> {
                EXPECT_EQ(args.name, "daily");
                EXPECT_EQ(
                    runs.get("daily")->state, BackupRunState::snapshotting
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
                EXPECT_EQ(runs.get("daily")->state, BackupRunState::uploading);
                return metadata;
            }
        ));
    EXPECT_CALL(fs, removeAll(_)).WillOnce(Return(Result<void>{}));
    work();
    EXPECT_EQ(runs.get("daily")->state, BackupRunState::completed);
    service->tick();
    EXPECT_FALSE(runs.has("daily"));
}

TEST_F(BackupServiceTest, TickRemovesOnlyCompletedRuns) {
    runs.put("completed", {.state = BackupRunState::completed});
    runs.put("starting", {.state = BackupRunState::starting});
    runs.put("snapshotting", {.state = BackupRunState::snapshotting});
    runs.put("uploading", {.state = BackupRunState::uploading});
    runs.put("failed", {.state = BackupRunState::failed});
    service->tick();
    EXPECT_FALSE(runs.has("completed"));
    for (const auto* name :
         {"starting", "snapshotting", "uploading", "failed"}) {
        EXPECT_TRUE(runs.has(name));
    }
    service->tick();
    EXPECT_TRUE(runs.has("failed"));
}

TEST(Ports, AccessorsPreserveInjectedObjects) {
    InMemoryBackupRunRepo runs;
    StrictMock<MockPlanRepo> plans;
    Snapshotters snapshotters;
    SnapshotStores stores;
    InlineExecutor executor;
    Ports ports{runs, plans, snapshotters, stores, executor};
    EXPECT_EQ(&ports.runRepo(), &runs);
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
