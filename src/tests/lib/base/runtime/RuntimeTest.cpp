#include <csignal>
#include <future>
#include <thread>

#include "TestSupport.hh"
#include "base/runtime/SignalHandler.hh"
#include "base/runtime/Thread.hh"
#include "base/runtime/ThreadSafeQueue.hh"
#include "core/services/RequestQueue.hh"

namespace anchor::test {
using namespace testing;
using namespace std::chrono_literals;

TEST(ThreadSafeQueue, EmptyQueueAndFifoWithMoveOnlyValues) {
    ThreadSafeQueue<std::unique_ptr<int>> queue;
    EXPECT_FALSE(queue.closed());
    EXPECT_FALSE(queue.tryPop());
    EXPECT_TRUE(queue.push(std::make_unique<int>(1)));
    EXPECT_TRUE(queue.push(std::make_unique<int>(2)));
    auto first = queue.tryPop();
    ASSERT_TRUE(first);
    EXPECT_EQ(**first, 1);
    auto second = queue.pop();
    ASSERT_TRUE(second);
    EXPECT_EQ(**second, 2);
    EXPECT_FALSE(queue.tryPop());
}

TEST(ThreadSafeQueue, ClosingRejectsPushesButAllowsDraining) {
    ThreadSafeQueue<int> queue;
    EXPECT_TRUE(queue.push(1));
    queue.close();
    queue.close();
    EXPECT_TRUE(queue.closed());
    EXPECT_FALSE(queue.push(2));
    EXPECT_EQ(queue.pop(), 1);
    EXPECT_FALSE(queue.pop());
    EXPECT_FALSE(queue.tryPop());
}

TEST(ThreadSafeQueue, PushWakesConsumerAndCloseWakesAllRemainingConsumers) {
    ThreadSafeQueue<int> queue;
    auto first = std::async(std::launch::async, [&] { return queue.pop(); });
    EXPECT_TRUE(queue.push(42));
    EXPECT_EQ(first.wait_for(1s), std::future_status::ready);
    queue.close();
    EXPECT_EQ(first.get(), 42);

    ThreadSafeQueue<int> empty;
    auto second = std::async(std::launch::async, [&] { return empty.pop(); });
    auto third = std::async(std::launch::async, [&] { return empty.pop(); });
    empty.close();
    EXPECT_EQ(second.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(third.wait_for(1s), std::future_status::ready);
    EXPECT_FALSE(second.get());
    EXPECT_FALSE(third.get());
}

TEST(ThreadSafeQueue, ConcurrentProducersDeliverEachValueOnce) {
    ThreadSafeQueue<int> queue;
    std::thread first{[&] {
        for (int i = 0; i < 100; ++i) {
            EXPECT_TRUE(queue.push(int{i}));
        }
    }};
    std::thread second{[&] {
        for (int i = 100; i < 200; ++i) {
            EXPECT_TRUE(queue.push(int{i}));
        }
    }};
    first.join();
    second.join();
    queue.close();
    std::vector<int> values;
    while (auto value = queue.pop()) {
        values.push_back(*value);
    }
    std::sort(values.begin(), values.end());
    ASSERT_EQ(values.size(), 200);
    for (int i = 0; i < 200; ++i) {
        EXPECT_EQ(values[i], i);
    }
}

TEST(RequestQueue, SendsReceivesAndRejectsRequestsAfterClosing) {
    RequestQueue queue;
    RequestSender sender{queue};
    RequestReceiver receiver{queue};
    EXPECT_FALSE(receiver.tryReceive());
    EXPECT_TRUE(sender.send(TriggerBackupRequest{"daily"}));
    auto request = receiver.tryReceive();
    ASSERT_TRUE(request);
    EXPECT_EQ(std::get<TriggerBackupRequest>(*request).name, "daily");
    queue.close();
    EXPECT_FALSE(sender.send(DiagnosticRequest{"details"}));
}

class CountingThread : public Thread {
   public:
    explicit CountingThread(std::atomic_int& count)
        : Thread("test"), count(count) {}

   private:
    void run() override { ++count; }
    std::atomic_int& count;
};

TEST(Thread, StartsAndJoinsThroughAllOverloads) {
    std::atomic_int count{0};
    CountingThread first{count}, second{count};
    first.join();
    first.start();
    second.start();
    Thread::join(first, second);
    EXPECT_EQ(count.load(), 2);
    first.join();
    first.start();
    second.start();
    std::vector<Thread*> threads{&first, &second};
    Thread::join(std::span<Thread*>{threads});
    EXPECT_EQ(count.load(), 4);
}

TEST(Signal, ConvertsSupportedPosixSignalsAndRejectsOthers) {
    EXPECT_EQ(*fromPosix(SIGINT), Signal::sigint);
    EXPECT_EQ(*fromPosix(SIGTERM), Signal::sigterm);
    EXPECT_EQ(toString(Signal::sigint), "SIGINT");
    EXPECT_EQ(toString(Signal::sigterm), "SIGTERM");
    auto unsupported = fromPosix(SIGUSR1);
    ASSERT_FALSE(unsupported);
    EXPECT_EQ(unsupported.error().code(), ErrorCode::invalidArgument);
}

TEST(SignalHandlerDeathTest, WaitsForSupportedSignalsUntilCallbackStops) {
    EXPECT_EXIT(
        {
            SignalHandler handler;
            int received = 0;
            if (std::raise(SIGINT) != 0) {
                std::_Exit(1);
            }
            handler.wait([&](Signal signal) {
                ++received;
                if (received == 1 && signal == Signal::sigint) {
                    if (std::raise(SIGTERM) != 0) {
                        std::_Exit(1);
                    }
                    return SignalAction::waitForNext;
                }
                if (received != 2 || signal != Signal::sigterm) {
                    std::_Exit(1);
                }
                return SignalAction::stop;
            });
            std::_Exit(received == 2 ? 0 : 1);
        },
        ExitedWithCode(0), ""
    );
}

}  // namespace anchor::test
