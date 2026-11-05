#include "TestSupport.hh"
#include "adapters/asio/AsioController.hh"
#include "core/Error.hh"
#include "proto/BackupMessages.hh"
#include "proto/SnapshotMessages.hh"
#include "proto/SystemMessages.hh"

namespace anchor::test {
using namespace kc;
using namespace std::chrono_literals;

TEST(AsioController, DiagnosticRequestsAreAcknowledgedAndQueued) {
    RequestQueue queue;
    AsioController controller{RequestSender{queue}};
    auto response = std::async(std::launch::async, [&] {
        return controller.handle(proto::DiagnosticRequest{"details"});
    });
    auto request = queue.popFor(1s);
    queue.close();
    ASSERT_TRUE(request);
    EXPECT_EQ(response.wait_for(0ms), std::future_status::timeout);
    request->promise.set_value(AckResponse{});
    ASSERT_EQ(response.wait_for(1s), std::future_status::ready);
    ASSERT_NE(response.get()->as<proto::AckResponse>(), nullptr);
    ASSERT_TRUE(std::holds_alternative<DiagnosticRequest>(request->request));
    EXPECT_EQ(std::get<DiagnosticRequest>(request->request).details, "details");
    EXPECT_FALSE(queue.tryPop());
}

TEST(AsioController, BackupRequestsAreAcknowledgedAndQueued) {
    RequestQueue queue;
    AsioController controller{RequestSender{queue}};
    auto response = std::async(std::launch::async, [&] {
        return controller.handle(proto::TriggerBackupRequest{"daily"});
    });
    auto request = queue.popFor(1s);
    queue.close();
    ASSERT_TRUE(request);
    EXPECT_EQ(response.wait_for(0ms), std::future_status::timeout);
    request->promise.set_value(AckResponse{});
    ASSERT_EQ(response.wait_for(1s), std::future_status::ready);
    ASSERT_NE(response.get()->as<proto::AckResponse>(), nullptr);
    ASSERT_TRUE(std::holds_alternative<TriggerBackupRequest>(request->request));
    EXPECT_EQ(std::get<TriggerBackupRequest>(request->request).name, "daily");
}

TEST(AsioController, ClosedQueueReturnsQueueErrorForBothRequests) {
    RequestQueue queue;
    queue.close();
    AsioController controller{RequestSender{queue}};
    auto diagnostic = controller.handle(proto::DiagnosticRequest{"details"});
    auto backup = controller.handle(proto::TriggerBackupRequest{"daily"});
    for (const auto* response : {diagnostic.get(), backup.get()}) {
        auto* error = response->as<proto::ErrorResponse>();
        ASSERT_NE(error, nullptr);
        EXPECT_EQ(error->error().code(), ErrorCode::queueError);
    }
    EXPECT_FALSE(queue.tryPop());
}

TEST(AsioController, RejectsResponsesAsRequestsWithoutEnqueuing) {
    RequestQueue queue;
    AsioController controller{RequestSender{queue}};
    auto response = controller.handle(proto::AckResponse{});
    auto* error = response->as<proto::ErrorResponse>();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->error().code(), ErrorCode::unknownRequest);
    EXPECT_FALSE(queue.tryPop());
}

TEST(AsioController, ReloadRequestsUseServiceQueueAndForwardLoadingErrors) {
    RequestQueue queue;
    AsioController controller{RequestSender{queue}};
    auto response = std::async(std::launch::async, [&] {
        return controller.handle(proto::ReloadPlansRequest{});
    });
    auto request = queue.popFor(1s);
    queue.close();
    ASSERT_TRUE(request);
    EXPECT_TRUE(std::holds_alternative<ReloadPlansRequest>(request->request));
    EXPECT_EQ(response.wait_for(0ms), std::future_status::timeout);
    request->promise.set_value(
        ErrorResponse{Error{kc::ErrorCode::invalidConfig, "bad plan"}}
    );
    ASSERT_EQ(response.wait_for(1s), std::future_status::ready);
    auto message = response.get();
    const auto* error = message->as<proto::ErrorResponse>();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->error().code(), kc::ErrorCode::invalidConfig);
    EXPECT_EQ(error->error().message(), "bad plan");
}

TEST(AsioController, ListRequestsReturnCurrentBackupsAndRunDetails) {
    RequestQueue queue;
    AsioController controller{RequestSender{queue}};
    auto response = std::async(std::launch::async, [&] {
        return controller.handle(proto::GetBackupsRequest{});
    });
    auto request = queue.popFor(1s);
    queue.close();
    ASSERT_TRUE(request);
    EXPECT_TRUE(std::holds_alternative<GetBackupsRequest>(request->request));
    request->promise.set_value(
        GetBackupsResponse{{Backup{
            .plan = plan(),
            .run = BackupRunStatus{
                .state = BackupRunState::snapshotting, .uuid = "active-run"
            }
        }}}
    );
    ASSERT_EQ(response.wait_for(1s), std::future_status::ready);
    auto message = response.get();
    const auto* listed = message->as<proto::GetBackupsResponse>();
    ASSERT_NE(listed, nullptr);
    ASSERT_EQ(listed->backups().size(), 1);
    ASSERT_TRUE(listed->backups()[0].run);
    EXPECT_EQ(listed->backups()[0].run->uuid, "active-run");
    EXPECT_EQ(listed->backups()[0].run->state, BackupRunState::snapshotting);
}

TEST(
    AsioController, SnapshotRequestsPreserveScopeAndIdsThroughTheServiceQueue
) {
    const Str uuid = "12345678-1234-4234-8234-123456789abc";
    for (const auto* name :
         {"GetSnapshotsRequest", "GetSnapshotRequest",
          "RemoveSnapshotRequest"}) {
        RequestQueue queue;
        AsioController controller{RequestSender{queue}};
        auto message = proto::Message::create(
            Json{{"name", name}, {"body", {{"name", "daily"}, {"uuid", uuid}}}}
        );
        ASSERT_TRUE(message);
        auto response = std::async(std::launch::async, [&] {
            return controller.handle(**message);
        });
        auto request = queue.popFor(1s);
        queue.close();
        ASSERT_TRUE(request);
        if (const auto* list =
                std::get_if<GetSnapshotsRequest>(&request->request)) {
            EXPECT_EQ(Str{name}, "GetSnapshotsRequest");
            EXPECT_EQ(list->backupName, "daily");
            request->promise.set_value(GetSnapshotsResponse{});
        } else if (
            const auto* inspect =
                std::get_if<GetSnapshotRequest>(&request->request)
        ) {
            EXPECT_EQ(Str{name}, "GetSnapshotRequest");
            EXPECT_EQ(inspect->backupName, "daily");
            EXPECT_EQ(inspect->uuid, uuid);
            request->promise.set_value(GetSnapshotResponse{});
        } else if (
            const auto* remove =
                std::get_if<RemoveSnapshotRequest>(&request->request)
        ) {
            EXPECT_EQ(Str{name}, "RemoveSnapshotRequest");
            EXPECT_EQ(remove->backupName, "daily");
            EXPECT_EQ(remove->uuid, uuid);
            request->promise.set_value(AckResponse{});
        } else {
            ADD_FAILURE() << "Unexpected request type";
            request->promise.set_value(
                ErrorResponse{Error{ErrorCode::unknownRequest}}
            );
        }
        ASSERT_EQ(response.wait_for(1s), std::future_status::ready);
        const auto json = response.get()->serialize();
        if (Str{name} == "GetSnapshotsRequest") {
            EXPECT_EQ(json["name"], "GetSnapshotsResponse");
            EXPECT_EQ(json["body"], Json::array());
        } else {
            EXPECT_EQ(
                json["name"], Str{name} == "GetSnapshotRequest"
                                  ? "GetSnapshotResponse"
                                  : "AckResponse"
            );
            EXPECT_TRUE(json["body"].is_null());
        }
    }
}

}  // namespace anchor::test
