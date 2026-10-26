#include "TestSupport.hh"
#include "adapters/asio/AsioController.hh"
#include "proto/AckResponse.hh"
#include "proto/DiagnosticRequest.hh"
#include "proto/ErrorResponse.hh"
#include "proto/TriggerBackupRequest.hh"

namespace anchor::test {
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

}  // namespace anchor::test
