#include <future>

#include "TestSupport.hh"
#include "adapters/backup/InMemoryBackupRepo.hh"
#include "adapters/executors/InlineExecutor.hh"
#include "core/Error.hh"
#include "core/services/BackupService.hh"

namespace anchor::test {
using namespace kc;
using namespace testing;
using namespace std::chrono_literals;

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
        loadService();
    }
    void TearDown() override { service.reset(); }
    void loadService() {
        service.reset();
        EXPECT_CALL(plans, forEach(_))
            .WillOnce(Invoke([&](auto callback) -> Result<void> {
                callback(backup);
                return {};
            }));
        service = std::make_unique<BackupService>(
            config, fs, Ports{backups, plans, snapshotters, stores, executor}
        );
        service->start();
    }
    Response send(Request request) {
        auto result = service->sender().send(std::move(request));
        EXPECT_TRUE(result);
        if (not result) {
            return ErrorResponse{result.error()};
        }
        const auto status = result->wait_for(2s);
        EXPECT_EQ(status, std::future_status::ready);
        if (status != std::future_status::ready) {
            return ErrorResponse{Error{kc::ErrorCode::timeout}};
        }
        return result->get();
    }
    void expectError(
        const Response& response, std::error_code code, const Str& message
    ) {
        auto* error = std::get_if<ErrorResponse>(&response);
        ASSERT_NE(error, nullptr);
        EXPECT_EQ(error->error.code(), code);
        EXPECT_EQ(error->error.message(), message);
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
    expectError(
        send(TriggerBackupRequest{"missing"}), kc::ErrorCode::invalidArgument,
        "Backup plan not found"
    );
    EXPECT_FALSE(backups.get("missing"));
}

TEST_F(BackupServiceTest, RepeatedShutdownJoinsWorkersBeforeDestruction) {
    for (int i = 0; i < 20; ++i) {
        loadService();
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
    loadService();
    auto response = send(GetBackupsRequest{});
    auto* listed = std::get_if<GetBackupsResponse>(&response);
    ASSERT_NE(listed, nullptr);
    ASSERT_EQ(listed->backups.size(), 1);
    const auto& stored = listed->backups.front();
    EXPECT_EQ(stored.plan.destination.str(), "local:///new-backups");
    EXPECT_FALSE(stored.enabled);
    ASSERT_TRUE(stored.run);
    EXPECT_EQ(stored.run->state, BackupRunState::uploading);
    EXPECT_EQ(stored.run->uuid, "existing-run");
}

TEST_F(BackupServiceTest, ActiveRunsRejectManualTriggersAndSkipScheduledOnes) {
    for (auto state :
         {BackupRunState::queued, BackupRunState::starting,
          BackupRunState::snapshotting, BackupRunState::uploading}) {
        backups.updateRunStatus(
            "daily", BackupRunStatus{.state = state, .uuid = "existing-run"}
        );
        expectError(
            send(TriggerBackupRequest{"daily"}), ErrorCode::alreadyRunning,
            "Backup is already running"
        );
        EXPECT_TRUE(
            std::holds_alternative<AckResponse>(
                send(TriggerBackupRequest{.name = "daily", .scheduled = true})
            )
        );
        EXPECT_EQ(backups.getRunStatus("daily")->state, state);
        EXPECT_EQ(backups.getRunStatus("daily")->uuid, "existing-run");
    }
}

TEST_F(BackupServiceTest, MissingSnapshotterDoesNotCreateRun) {
    addStore();
    expectError(
        send(TriggerBackupRequest{"daily"}), kc::ErrorCode::invalidArgument,
        "Snapshotter not found"
    );
    EXPECT_FALSE(backups.getRunStatus("daily"));
    EXPECT_TRUE(backups.get("daily"));
}

TEST_F(BackupServiceTest, MissingStoreDoesNotCreateRun) {
    addSnapshotter();
    expectError(
        send(TriggerBackupRequest{"daily"}), kc::ErrorCode::invalidArgument,
        "Snapshot store not found"
    );
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
            EXPECT_TRUE(uuids::uuid::is_valid_uuid(status->uuid));
        }
        submitted.set_value(std::move(callback));
    }));
    auto response = send(TriggerBackupRequest{"daily"});
    auto duplicate = send(TriggerBackupRequest{"daily"});
    const auto workStatus = work.wait_for(1s);
    service.reset();
    EXPECT_TRUE(std::holds_alternative<AckResponse>(response));
    ASSERT_EQ(workStatus, std::future_status::ready);
    EXPECT_TRUE(work.get());
    expectError(
        duplicate, ErrorCode::alreadyRunning, "Backup is already running"
    );
}

TEST_F(BackupServiceTest, DisabledBackupsSkipSchedulingButAllowManualTriggers) {
    backups.put(Backup{.plan = backup, .enabled = false});
    EXPECT_TRUE(
        std::holds_alternative<AckResponse>(
            send(TriggerBackupRequest{.name = "daily", .scheduled = true})
        )
    );
    EXPECT_FALSE(backups.getRunStatus("daily"));
    addSnapshotter();
    addStore();
    EXPECT_CALL(executor, call(_));
    EXPECT_TRUE(
        std::holds_alternative<AckResponse>(send(TriggerBackupRequest{"daily"}))
    );
    ASSERT_TRUE(backups.getRunStatus("daily"));
    EXPECT_EQ(backups.getRunStatus("daily")->state, BackupRunState::queued);
}

TEST_F(BackupServiceTest, ManualAndScheduledProducersShareSerializedAdmission) {
    addSnapshotter();
    addStore();
    EXPECT_CALL(executor, call(_));
    auto manualSender = service->sender();
    auto scheduledSender = service->sender();
    auto scheduled = scheduledSender.send(
        TriggerBackupRequest{.name = "daily", .scheduled = true}
    );
    auto manual = manualSender.send(TriggerBackupRequest{"daily"});
    ASSERT_TRUE(scheduled);
    ASSERT_TRUE(manual);
    ASSERT_EQ(scheduled->wait_for(1s), std::future_status::ready);
    ASSERT_EQ(manual->wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(std::holds_alternative<AckResponse>(scheduled->get()));
    expectError(
        manual->get(), ErrorCode::alreadyRunning, "Backup is already running"
    );
}

TEST_F(BackupServiceTest, TerminalResultsRemainVisibleAndAllowAnotherRun) {
    addSnapshotter();
    addStore();
    EXPECT_CALL(executor, call(_)).Times(2);
    for (auto state : {BackupRunState::completed, BackupRunState::failed}) {
        backups.updateRunStatus(
            "daily",
            BackupRunStatus{
                .state = state,
                .error = state == BackupRunState::failed
                             ? Opt<Error>{Error{
                                   kc::ErrorCode::fileSystemError, "disk full"
                               }}
                             : Opt<Error>{},
                .uuid = "previous-run"
            }
        );
        auto response = send(GetBackupsRequest{});
        auto* listed = std::get_if<GetBackupsResponse>(&response);
        ASSERT_NE(listed, nullptr);
        ASSERT_EQ(listed->backups.size(), 1);
        ASSERT_TRUE(listed->backups.front().run);
        EXPECT_EQ(listed->backups.front().run->state, state);
        EXPECT_EQ(listed->backups.front().run->uuid, "previous-run");
        EXPECT_TRUE(
            std::holds_alternative<AckResponse>(
                send(TriggerBackupRequest{.name = "daily", .scheduled = true})
            )
        );
        const auto status = backups.getRunStatus("daily");
        ASSERT_TRUE(status);
        EXPECT_EQ(status->state, BackupRunState::queued);
        EXPECT_NE(status->uuid, "previous-run");
        EXPECT_FALSE(status->error);
    }
}

TEST_F(BackupServiceTest, ShutdownDrainsRequestsAndQueuedBackupWork) {
    auto* snapshotter = addSnapshotter();
    auto* store = addStore();
    EXPECT_CALL(executor, call(_)).WillOnce(Invoke([](auto callback) {
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
    auto sender = service->sender();
    auto trigger = sender.send(TriggerBackupRequest{"daily"});
    std::vector<std::future<Response>> responses;
    for (int i = 0; i < 20; ++i) {
        auto response = sender.send(DiagnosticRequest{"details"});
        ASSERT_TRUE(response);
        responses.push_back(std::move(*response));
    }
    service->stop();
    service->join();
    ASSERT_TRUE(trigger);
    ASSERT_EQ(trigger->wait_for(0ms), std::future_status::ready);
    EXPECT_TRUE(std::holds_alternative<AckResponse>(trigger->get()));
    for (auto& response : responses) {
        ASSERT_EQ(response.wait_for(0ms), std::future_status::ready);
        EXPECT_TRUE(std::holds_alternative<AckResponse>(response.get()));
    }
    const auto status = backups.getRunStatus("daily");
    ASSERT_TRUE(status);
    EXPECT_EQ(status->state, BackupRunState::completed);
    auto rejected = sender.send(GetBackupsRequest{});
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error().code(), ErrorCode::queueError);
}

TEST_F(BackupServiceTest, AcceptedRunKeepsItsPlanSnapshot) {
    auto* snapshotter = addSnapshotter();
    auto* store = addStore();
    std::promise<MoveOnlyFunction<void()>> submitted;
    auto work = submitted.get_future();
    EXPECT_CALL(executor, call(_)).WillOnce(Invoke([&](auto callback) {
        submitted.set_value(std::move(callback));
    }));
    auto current = backups.get("daily").value();
    current.plan.destination = Location::local("/accepted");
    backups.put(current);
    auto response =
        send(TriggerBackupRequest{.name = "daily", .scheduled = true});
    auto workStatus = work.wait_for(1s);
    ASSERT_TRUE(std::holds_alternative<AckResponse>(response));
    ASSERT_EQ(workStatus, std::future_status::ready);
    backup.destination = Location::local("/changed-after-acceptance");
    EXPECT_CALL(plans, forEach(_))
        .WillOnce(Invoke([&](auto callback) -> Result<void> {
            callback(backup);
            return {};
        }));
    EXPECT_TRUE(
        std::holds_alternative<AckResponse>(send(ReloadPlansRequest{}))
    );
    EXPECT_EQ(
        backups.get("daily")->plan.destination.str(),
        "local:///changed-after-acceptance"
    );
    EXPECT_EQ(backups.getRunStatus("daily")->state, BackupRunState::queued);
    service.reset();
    EXPECT_CALL(fs, createDirectory(_)).WillOnce(Return(Result<void>{}));
    EXPECT_CALL(fs, glob("/source/file.txt"))
        .WillOnce(Return(std::vector<Path>{"/source/file.txt"}));
    EXPECT_CALL(fs, isDirectory(_)).WillOnce(Return(false));
    SnapshotMetadata metadata{"snapshot", Location::local("/snapshot")};
    EXPECT_CALL(*snapshotter, snapshot(_)).WillOnce(Return(metadata));
    EXPECT_CALL(*store, put(_, _))
        .WillOnce(Invoke(
            [&](const SnapshotMetadata&,
                const Location& destination) -> Result<SnapshotMetadata> {
                EXPECT_EQ(destination.str(), "local:///accepted");
                return metadata;
            }
        ));
    EXPECT_CALL(fs, removeAll(_)).WillOnce(Return(Result<void>{}));
    auto callback = work.get();
    callback();
    EXPECT_EQ(backups.getRunStatus("daily")->state, BackupRunState::completed);
}

TEST_F(BackupServiceTest, RealSchedulerSubmitsIntoOwnedQueue) {
    backup.cron = *Cron::fromString("* * * * * *");
    addSnapshotter();
    addStore();
    std::promise<void> attempted;
    auto ready = attempted.get_future();
    EXPECT_CALL(executor, call(_)).WillRepeatedly(Invoke([](auto callback) {
        callback();
    }));
    auto failure =
        Error::unexpected(kc::ErrorCode::fileSystemError, "mkdir failed");
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([&](const Path&) -> Result<void> {
            attempted.set_value();
            return failure;
        }))
        .WillRepeatedly(Return(failure));
    loadService();
    auto status = ready.wait_for(3s);
    service.reset();
    ASSERT_EQ(status, std::future_status::ready);
    ASSERT_TRUE(backups.getRunStatus("daily"));
    EXPECT_EQ(backups.getRunStatus("daily")->state, BackupRunState::failed);
}

TEST_F(
    BackupServiceTest, ReloadAddsAndUpdatesPlansAndKeepsMissingPlansAndRunState
) {
    auto retained = backup;
    retained.name = "deleted-on-disk";
    backups.putPlan(retained);
    backups.put(
        Backup{
            .plan = backup,
            .enabled = false,
            .run = BackupRunStatus{
                .state = BackupRunState::uploading, .uuid = "active-id"
            }
        }
    );
    auto added = backup;
    added.name = "new";
    backup.destination = Location::local("/new-location");
    EXPECT_CALL(plans, forEach(_))
        .WillOnce(Invoke([&](auto callback) -> Result<void> {
            callback(backup);
            callback(added);
            return {};
        }));
    EXPECT_TRUE(
        std::holds_alternative<AckResponse>(send(ReloadPlansRequest{}))
    );
    const auto current = backups.get("daily");
    ASSERT_TRUE(current);
    EXPECT_EQ(current->plan.destination.str(), "local:///new-location");
    EXPECT_FALSE(current->enabled);
    ASSERT_TRUE(current->run);
    EXPECT_EQ(current->run->state, BackupRunState::uploading);
    EXPECT_EQ(current->run->uuid, "active-id");
    EXPECT_TRUE(backups.get("new"));
    EXPECT_TRUE(backups.get("deleted-on-disk"));
}

TEST_F(
    BackupServiceTest,
    ReloadReportsErrorsAfterApplyingValidPlansAndPreservesOtherPlans
) {
    auto valid = backup;
    valid.name = "valid";
    EXPECT_CALL(plans, forEach(_))
        .WillOnce(Invoke([&](auto callback) -> Result<void> {
            callback(valid);
            return Error::unexpected(
                kc::ErrorCode::invalidConfig,
                "Could not load plan 'daily.yaml': invalid cron"
            );
        }));
    expectError(
        send(ReloadPlansRequest{}), kc::ErrorCode::invalidConfig,
        "Could not load plan 'daily.yaml': invalid cron"
    );
    EXPECT_TRUE(backups.get("valid"));
    ASSERT_TRUE(backups.get("daily"));
    EXPECT_EQ(
        backups.get("daily")->plan.destination.str(), backup.destination.str()
    );
    EXPECT_CALL(plans, forEach(_))
        .WillOnce(Return(
            Error::unexpected(kc::ErrorCode::fileSystemError, "listing failed")
        ));
    expectError(
        send(ReloadPlansRequest{}), kc::ErrorCode::fileSystemError,
        "listing failed"
    );
    EXPECT_EQ(backups.getAll().size(), 2);
}

TEST_F(BackupServiceTest, ReloadWakesSchedulerWhenAddingFirstCron) {
    addSnapshotter();
    addStore();
    std::promise<void> attempted;
    auto ready = attempted.get_future();
    EXPECT_CALL(executor, call(_)).WillRepeatedly(Invoke([](auto callback) {
        callback();
    }));
    auto failure =
        Error::unexpected(kc::ErrorCode::fileSystemError, "mkdir failed");
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([&](const Path&) -> Result<void> {
            attempted.set_value();
            return failure;
        }))
        .WillRepeatedly(Return(failure));
    backup.cron = *Cron::fromString("* * * * * *");
    EXPECT_CALL(plans, forEach(_))
        .WillOnce(Invoke([&](auto callback) -> Result<void> {
            callback(backup);
            return {};
        }));
    EXPECT_TRUE(
        std::holds_alternative<AckResponse>(send(ReloadPlansRequest{}))
    );
    const auto status = ready.wait_for(3s);
    service.reset();
    EXPECT_EQ(status, std::future_status::ready);
    ASSERT_TRUE(backups.getRunStatus("daily"));
    EXPECT_EQ(backups.getRunStatus("daily")->state, BackupRunState::failed);
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
