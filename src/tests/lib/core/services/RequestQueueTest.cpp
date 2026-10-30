#include "TestSupport.hh"
#include "core/Error.hh"
#include "core/services/RequestQueue.hh"
namespace anchor::test {
using namespace kc;
using namespace testing;
using namespace std::chrono_literals;
TEST(RequestQueue, SendsReceivesAndRejectsRequestsAfterClosing) {
    RequestQueue queue;
    RequestSender sender{queue};
    EXPECT_FALSE(queue.tryPop());
    auto response = sender.send(TriggerBackupRequest{"daily"});
    ASSERT_TRUE(response);
    EXPECT_TRUE(response->valid());
    EXPECT_EQ(response->wait_for(0ms), std::future_status::timeout);
    auto request = queue.tryPop();
    ASSERT_TRUE(request);
    ASSERT_TRUE(std::holds_alternative<TriggerBackupRequest>(request->request));
    EXPECT_EQ(std::get<TriggerBackupRequest>(request->request).name, "daily");
    request->promise.set_value(AckResponse{});
    ASSERT_EQ(response->wait_for(0ms), std::future_status::ready);
    EXPECT_TRUE(std::holds_alternative<AckResponse>(response->get()));
    queue.close();
    auto rejected = sender.send(DiagnosticRequest{"details"});
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error().code(), kc::ErrorCode::queueError);
    EXPECT_FALSE(queue.tryPop());
}

TEST(RequestQueue, DeliversResponsesToMatchingSenders) {
    RequestQueue queue;
    RequestSender sender{queue};
    auto diagnostic = sender.send(DiagnosticRequest{"details"});
    auto backup = sender.send(TriggerBackupRequest{"daily"});
    ASSERT_TRUE(diagnostic);
    ASSERT_TRUE(backup);
    auto first = queue.tryPop();
    auto second = queue.tryPop();
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(std::holds_alternative<DiagnosticRequest>(first->request));
    EXPECT_EQ(std::get<DiagnosticRequest>(first->request).details, "details");
    ASSERT_TRUE(std::holds_alternative<TriggerBackupRequest>(second->request));
    EXPECT_EQ(std::get<TriggerBackupRequest>(second->request).name, "daily");
    second->promise.set_value(
        ErrorResponse{Error{kc::ErrorCode::invalidArgument, "bad plan"}}
    );
    EXPECT_EQ(diagnostic->wait_for(0ms), std::future_status::timeout);
    ASSERT_EQ(backup->wait_for(0ms), std::future_status::ready);
    const auto response = backup->get();
    ASSERT_TRUE(std::holds_alternative<ErrorResponse>(response));
    EXPECT_EQ(
        std::get<ErrorResponse>(response).error.code(),
        kc::ErrorCode::invalidArgument
    );
    EXPECT_EQ(std::get<ErrorResponse>(response).error.message(), "bad plan");
    first->promise.set_value(AckResponse{});
    ASSERT_EQ(diagnostic->wait_for(0ms), std::future_status::ready);
    EXPECT_TRUE(std::holds_alternative<AckResponse>(diagnostic->get()));
}

}  // namespace anchor::test
