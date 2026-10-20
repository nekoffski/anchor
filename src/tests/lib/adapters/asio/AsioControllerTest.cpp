#include "TestSupport.hh"
#include "adapters/asio/AsioController.hh"
#include "proto/AckResponse.hh"
#include "proto/DiagnosticRequest.hh"
#include "proto/ErrorResponse.hh"
#include "proto/TriggerBackupRequest.hh"

namespace anchor::test {

TEST(AsioController, DiagnosticRequestsAreAcknowledgedAndQueued) {
    RequestQueue queue;
    AsioController controller{RequestSender{queue}};
    auto response = controller.handle(proto::DiagnosticRequest{"details"});
    ASSERT_NE(response->as<proto::AckResponse>(), nullptr);
    auto request = RequestReceiver{queue}.tryReceive();
    ASSERT_TRUE(request);
    ASSERT_TRUE(std::holds_alternative<DiagnosticRequest>(request->request));
    EXPECT_EQ(std::get<DiagnosticRequest>(request->request).details, "details");
    EXPECT_FALSE(queue.tryPop());
}

TEST(AsioController, BackupRequestsAreAcknowledgedAndQueued) {
    RequestQueue queue;
    AsioController controller{RequestSender{queue}};
    auto response = controller.handle(proto::TriggerBackupRequest{"daily"});
    ASSERT_NE(response->as<proto::AckResponse>(), nullptr);
    auto request = queue.tryPop();
    ASSERT_TRUE(request);
    ASSERT_TRUE(std::holds_alternative<TriggerBackupRequest>(request->request));
    EXPECT_EQ(std::get<TriggerBackupRequest>(request->request).name, "daily");
}

TEST(AsioController, ClosedQueueReturnsConnectionFailureForBothRequests) {
    RequestQueue queue;
    queue.close();
    AsioController controller{RequestSender{queue}};
    auto diagnostic = controller.handle(proto::DiagnosticRequest{"details"});
    auto backup = controller.handle(proto::TriggerBackupRequest{"daily"});
    for (const auto* response : {diagnostic.get(), backup.get()}) {
        auto* error = response->as<proto::ErrorResponse>();
        ASSERT_NE(error, nullptr);
        EXPECT_EQ(error->error().code(), ErrorCode::connectionFailure);
    }
    EXPECT_FALSE(queue.tryPop());
}

TEST(AsioController, RejectsResponsesAsRequestsWithoutEnqueuing) {
    RequestQueue queue;
    AsioController controller{RequestSender{queue}};
    auto response = controller.handle(proto::AckResponse{});
    auto* error = response->as<proto::ErrorResponse>();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->error().code(), ErrorCode::invalidRequest);
    EXPECT_FALSE(queue.tryPop());
}

}  // namespace anchor::test
