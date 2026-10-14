#include "TestSupport.hh"
#include "adapters/backup/InMemoryBackupRunRepo.hh"
#include "core/services/BackupServiceRunner.hh"

namespace anchor::test {
using namespace testing;

TEST(BackupServiceRunner, DispatchesQueuedBackupRequestAndStops) {
    StrictMock<MockPlanRepo> plans;
    InMemoryBackupRunRepo runs;
    StrictMock<MockExecutor> executor;
    StrictMock<MockFileSystem> fs;
    Snapshotters snapshotters;
    SnapshotStores stores;
    snapshotters.add<StrictMock<MockSnapshotter>>(ArchiveType::directory);
    stores.add<StrictMock<MockSnapshotStore>>(LocationType::local);
    auto backup = plan();
    EXPECT_CALL(plans, forEach(_)).WillOnce(Invoke([&](auto callback) {
        callback(backup);
    }));
    BackupService service{
        Config{}, fs, Ports{runs, plans, snapshotters, stores, executor}
    };
    RequestQueue queue;
    RequestSender sender{queue};
    ASSERT_TRUE(sender.send(DiagnosticRequest{"details"}));
    ASSERT_TRUE(sender.send(TriggerBackupRequest{"daily"}));
    BackupServiceRunner runner{service, RequestReceiver{queue}};
    EXPECT_CALL(executor, call(_)).WillOnce(Invoke([&](auto) {
        runner.stop();
    }));
    runner.start();
    runner.join();
    auto status = runs.get("daily");
    ASSERT_TRUE(status);
    EXPECT_EQ(status->state, BackupRunState::starting);
    EXPECT_FALSE(queue.tryPop());
}

TEST(BackupServiceRunner, StoppingBeforeStartLeavesRequestsQueued) {
    StrictMock<MockPlanRepo> plans;
    InMemoryBackupRunRepo runs;
    StrictMock<MockExecutor> executor;
    StrictMock<MockFileSystem> fs;
    Snapshotters snapshotters;
    SnapshotStores stores;
    EXPECT_CALL(plans, forEach(_));
    BackupService service{
        Config{}, fs, Ports{runs, plans, snapshotters, stores, executor}
    };
    RequestQueue queue;
    ASSERT_TRUE(queue.push(TriggerBackupRequest{"daily"}));
    BackupServiceRunner runner{service, RequestReceiver{queue}};
    runner.stop();
    runner.start();
    runner.join();
    EXPECT_TRUE(queue.tryPop());
}

}  // namespace anchor::test
