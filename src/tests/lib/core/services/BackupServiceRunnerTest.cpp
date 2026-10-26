#include "TestSupport.hh"
#include "adapters/backup/InMemoryBackupRepo.hh"
#include "core/services/BackupServiceRunner.hh"

namespace anchor::test {
using namespace testing;
using namespace std::chrono_literals;

TEST(BackupServiceRunner, DispatchesQueuedRequestsAndResolvesResponses) {
    StrictMock<MockPlanRepo> plans;
    InMemoryBackupRepo backups;
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
    Config config;
    BackupService service{
        config, fs, Ports{backups, plans, snapshotters, stores, executor}
    };
    RequestQueue queue;
    RequestSender sender{queue};
    auto diagnostic = sender.send(DiagnosticRequest{"details"});
    auto backupResponse = sender.send(TriggerBackupRequest{"daily"});
    auto duplicate = sender.send(TriggerBackupRequest{"daily"});
    auto missing = sender.send(TriggerBackupRequest{"missing"});
    ASSERT_TRUE(diagnostic);
    ASSERT_TRUE(backupResponse);
    ASSERT_TRUE(duplicate);
    ASSERT_TRUE(missing);
    BackupServiceRunner runner{service, RequestReceiver{queue}};
    EXPECT_CALL(executor, call(_));
    runner.start();
    const auto diagnosticStatus = diagnostic->wait_for(1s);
    const auto backupStatus = backupResponse->wait_for(1s);
    const auto duplicateStatus = duplicate->wait_for(1s);
    const auto missingStatus = missing->wait_for(1s);
    runner.stop();
    runner.join();
    ASSERT_EQ(diagnosticStatus, std::future_status::ready);
    ASSERT_EQ(backupStatus, std::future_status::ready);
    ASSERT_EQ(duplicateStatus, std::future_status::ready);
    ASSERT_EQ(missingStatus, std::future_status::ready);
    EXPECT_TRUE(std::holds_alternative<AckResponse>(diagnostic->get()));
    EXPECT_TRUE(std::holds_alternative<AckResponse>(backupResponse->get()));
    const auto duplicateResponse = duplicate->get();
    ASSERT_TRUE(std::holds_alternative<ErrorResponse>(duplicateResponse));
    EXPECT_EQ(
        std::get<ErrorResponse>(duplicateResponse).error.code(),
        ErrorCode::alreadyRunning
    );
    const auto missingResponse = missing->get();
    ASSERT_TRUE(std::holds_alternative<ErrorResponse>(missingResponse));
    EXPECT_EQ(
        std::get<ErrorResponse>(missingResponse).error.code(),
        ErrorCode::invalidArgument
    );
    auto status = backups.getRunStatus("daily");
    ASSERT_TRUE(status);
    EXPECT_EQ(status->state, BackupRunState::queued);
    EXPECT_FALSE(queue.tryPop());
}

TEST(BackupServiceRunner, StoppingBeforeStartLeavesRequestsQueued) {
    StrictMock<MockPlanRepo> plans;
    InMemoryBackupRepo backups;
    StrictMock<MockExecutor> executor;
    StrictMock<MockFileSystem> fs;
    Snapshotters snapshotters;
    SnapshotStores stores;
    EXPECT_CALL(plans, forEach(_));
    Config config;
    BackupService service{
        config, fs, Ports{backups, plans, snapshotters, stores, executor}
    };
    RequestQueue queue;
    auto response = RequestSender{queue}.send(TriggerBackupRequest{"daily"});
    ASSERT_TRUE(response);
    BackupServiceRunner runner{service, RequestReceiver{queue}};
    runner.stop();
    runner.start();
    runner.join();
    EXPECT_EQ(response->wait_for(0ms), std::future_status::timeout);
    auto pending = queue.tryPop();
    ASSERT_TRUE(pending);
    ASSERT_TRUE(std::holds_alternative<TriggerBackupRequest>(pending->request));
    EXPECT_EQ(std::get<TriggerBackupRequest>(pending->request).name, "daily");
    pending->promise.set_value(AckResponse{});
    ASSERT_EQ(response->wait_for(0ms), std::future_status::ready);
    EXPECT_TRUE(std::holds_alternative<AckResponse>(response->get()));
    EXPECT_FALSE(queue.tryPop());
}

}  // namespace anchor::test
