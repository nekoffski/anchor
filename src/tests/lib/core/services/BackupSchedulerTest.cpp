#include "TestSupport.hh"
#include "adapters/backup/InMemoryBackupRepo.hh"
#include "core/services/BackupScheduler.hh"

namespace anchor::test {
using namespace testing;
using namespace std::chrono;
using namespace std::chrono_literals;

class BackupSchedulerTest : public Test {
   protected:
    InMemoryBackupRepo backups;
    RequestQueue requests;
    BackupScheduler scheduler{backups, RequestSender{requests}};
    static constexpr sys_seconds now{1'700'000'000s};

    void SetUp() override {
        auto backup = plan();
        backup.cron = *Cron::fromString("* * * * * *");
        backups.put(Backup{backup});
        scheduler.add("daily", now);
    }
    void expectRequest(const Str& name = "daily") {
        auto request = requests.tryPop();
        ASSERT_TRUE(request);
        auto* trigger = std::get_if<TriggerBackupRequest>(&request->request);
        ASSERT_NE(trigger, nullptr);
        EXPECT_EQ(trigger->name, name);
        EXPECT_TRUE(trigger->scheduled);
        request->promise.set_value(AckResponse{});
    }
};

TEST_F(BackupSchedulerTest, StartsInTheFutureAndSubmitsOncePerOccurrence) {
    scheduler.tick(now);
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(now + 1s);
    expectRequest();
    scheduler.tick(now + 1s);
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(now + 2s);
    expectRequest();
}

TEST_F(BackupSchedulerTest, SkipsMissedOccurrencesAfterSleepOrClockJump) {
    scheduler.tick(now);
    scheduler.tick(now + 120s);
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(now + 120s);
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(now + 121s);
    expectRequest();
}

TEST_F(BackupSchedulerTest, ResumesAfterClockMovesBackward) {
    scheduler.tick(now);
    scheduler.tick(now - 60s);
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(now - 59s);
    expectRequest();
}

TEST_F(BackupSchedulerTest, SkipsBusyOccurrencesWithoutDeferringThem) {
    scheduler.tick(now);
    for (auto state :
         {BackupRunState::queued, BackupRunState::starting,
          BackupRunState::snapshotting, BackupRunState::uploading}) {
        backups.updateRunStatus("daily", BackupRunStatus{.state = state});
        scheduler.tick(now + 1s);
        EXPECT_FALSE(requests.tryPop());
        backups.clearRunStatus("daily");
        scheduler.tick(now + 1s);
        EXPECT_FALSE(requests.tryPop());
        // Reset the clock to exercise each active state at a fresh occurrence.
        scheduler.tick(now);
    }
    scheduler.tick(now + 1s);
    expectRequest();
}

TEST_F(BackupSchedulerTest, TerminalResultsDoNotBlockScheduling) {
    scheduler.tick(now);
    for (auto state : {BackupRunState::completed, BackupRunState::failed}) {
        backups.updateRunStatus("daily", BackupRunStatus{.state = state});
        scheduler.tick(now + 1s);
        expectRequest();
        scheduler.tick(now);
    }
}

TEST_F(BackupSchedulerTest, DisabledAndManualOnlyBackupsDoNotSubmitRequests) {
    auto disabled = backups.get("daily").value();
    disabled.enabled = false;
    backups.put(disabled);
    auto manual = plan();
    manual.name = "manual";
    backups.put(Backup{manual});
    scheduler.tick(now);
    scheduler.tick(now + 1s);
    EXPECT_FALSE(requests.tryPop());
    disabled.enabled = true;
    backups.put(disabled);
    scheduler.tick(now + 1s);
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(now + 2s);
    expectRequest();
    EXPECT_FALSE(requests.tryPop());
}

TEST_F(BackupSchedulerTest, KeepsSchedulesForDifferentBackupsIndependent) {
    auto second = backups.get("daily").value();
    second.plan.name = "second";
    backups.put(second);
    scheduler.add("second", now);
    backups.updateRunStatus(
        "daily", BackupRunStatus{.state = BackupRunState::uploading}
    );
    scheduler.tick(now);
    scheduler.tick(now + 1s);
    expectRequest("second");
    EXPECT_FALSE(requests.tryPop());
}

TEST_F(BackupSchedulerTest, StoppingBeforeStartExitsAndDoesNotSubmit) {
    scheduler.stop();
    scheduler.add("daily", now);
    scheduler.start();
    scheduler.join();
    EXPECT_FALSE(requests.tryPop());
}

TEST_F(
    BackupSchedulerTest, ProcessesTheEarliestDeadlineRegardlessOfInsertionOrder
) {
    auto late = backups.get("daily").value();
    late.plan.cron = *Cron::fromString("*/5 * * * * *");
    backups.put(late);
    scheduler.add("daily", now);
    auto early = late;
    early.plan.name = "early";
    early.plan.cron = *Cron::fromString("*/2 * * * * *");
    backups.put(early);
    scheduler.add("early", now);
    scheduler.tick(now + 2s);
    expectRequest("early");
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(now + 5s);
    expectRequest("daily");
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(now + 6s);
    expectRequest("early");
    EXPECT_FALSE(requests.tryPop());
}

TEST(BackupScheduler, StopInterruptsALongDeadlineWait) {
    StrictMock<MockBackupRepo> backups;
    RequestQueue requests;
    auto backup = Backup{plan()};
    backup.plan.cron = *Cron::fromString("0 0 0 1 1 *");
    std::promise<void> initialized;
    auto ready = initialized.get_future();
    EXPECT_CALL(backups, get("daily"))
        .WillOnce(Return(backup))
        .WillOnce(Invoke([&](const Str&) {
            initialized.set_value();
            return backup;
        }));
    BackupScheduler scheduler{backups, RequestSender{requests}};
    scheduler.add("daily");
    scheduler.start();
    const auto status = ready.wait_for(2s);
    scheduler.stop();
    auto joined = std::async(std::launch::async, [&] { scheduler.join(); });
    EXPECT_EQ(joined.wait_for(200ms), std::future_status::ready);
    joined.get();
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_FALSE(requests.tryPop());
}

TEST_F(BackupSchedulerTest, ReAddingReplacesTheDeadlineWithoutDuplicatingIt) {
    auto backup = backups.get("daily").value();
    backup.plan.cron = *Cron::fromString("*/5 * * * * *");
    backups.put(backup);
    scheduler.add("daily", now);
    scheduler.add("daily", now);
    scheduler.tick(now + 1s);
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(now + 5s);
    expectRequest();
    EXPECT_FALSE(requests.tryPop());
}

TEST_F(BackupSchedulerTest, RemovingCronRemovesTheRegisteredDeadline) {
    auto backup = backups.get("daily").value();
    backup.plan.cron.reset();
    backups.put(backup);
    scheduler.add("daily", now);
    scheduler.tick(now + 1s);
    EXPECT_FALSE(requests.tryPop());
}

TEST(BackupScheduler, AddingAnEarlierScheduleInterruptsTheCurrentWait) {
    StrictMock<MockBackupRepo> backups;
    RequestQueue requests;
    auto late = Backup{plan()};
    late.plan.name = "late";
    late.plan.cron = *Cron::fromString("0 0 0 1 1 *");
    auto early = Backup{plan()};
    early.plan.name = "early";
    early.plan.cron = *Cron::fromString("* * * * * *");
    std::promise<void> initialized;
    auto ready = initialized.get_future();
    EXPECT_CALL(backups, get("late"))
        .WillOnce(Return(late))
        .WillOnce(Invoke([&](const Str&) {
            initialized.set_value();
            return late;
        }));
    EXPECT_CALL(backups, get("early")).WillRepeatedly(Return(early));
    BackupScheduler scheduler{backups, RequestSender{requests}};
    scheduler.add("late");
    scheduler.start();
    const auto initializedStatus = ready.wait_for(2s);
    scheduler.add("early");
    auto request = requests.popFor(2s);
    scheduler.stop();
    scheduler.join();
    EXPECT_EQ(initializedStatus, std::future_status::ready);
    ASSERT_TRUE(request);
    const auto* trigger = std::get_if<TriggerBackupRequest>(&request->request);
    ASSERT_NE(trigger, nullptr);
    EXPECT_EQ(trigger->name, "early");
    EXPECT_TRUE(trigger->scheduled);
    request->promise.set_value(AckResponse{});
}

TEST(BackupScheduler, EmptyQueueSleepsUntilAScheduleIsAdded) {
    InMemoryBackupRepo backups;
    RequestQueue requests;
    auto backup = plan();
    backup.cron = *Cron::fromString("* * * * * *");
    backups.put(Backup{backup});
    BackupScheduler scheduler{backups, RequestSender{requests}};
    scheduler.start();
    auto idle = requests.popFor(50ms);
    EXPECT_FALSE(idle);
    scheduler.add("daily");
    auto request = requests.popFor(2s);
    scheduler.stop();
    scheduler.join();
    ASSERT_TRUE(request);
    const auto* trigger = std::get_if<TriggerBackupRequest>(&request->request);
    ASSERT_NE(trigger, nullptr);
    EXPECT_EQ(trigger->name, "daily");
    request->promise.set_value(AckResponse{});
}

TEST_F(BackupSchedulerTest, DailyScheduleContinuesAcrossSpringClockChange) {
    DEFER { tzset(); };
    ScopedEnv timezone{"TZ", "Europe/Warsaw"};
    tzset();
    auto backup = backups.get("daily").value();
    backup.plan.cron = *Cron::fromString("0 0 2 * * *");
    backups.put(backup);
    scheduler.tick(sys_days{2024y / March / 30} + 2h);
    const sys_seconds due = sys_days{2024y / April / 1};
    scheduler.tick(due - 1s);
    EXPECT_FALSE(requests.tryPop());
    scheduler.tick(due);
    expectRequest();
}

}  // namespace anchor::test
