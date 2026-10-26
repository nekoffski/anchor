#include <variant>

#include "TestSupport.hh"
#include "base/Functional.hh"

namespace anchor::test {
using namespace testing;

TEST(Pipeline, RunsStepsInOrder) {
    std::vector<int> calls;
    auto result = pipeline(
        [&]() -> Result<void> {
            calls.push_back(1);
            return {};
        },
        [&]() -> Result<void> {
            calls.push_back(2);
            return {};
        },
        [&]() -> Result<void> {
            calls.push_back(3);
            return {};
        }
    );
    EXPECT_TRUE(result);
    EXPECT_THAT(calls, ElementsAre(1, 2, 3));
}

TEST(Pipeline, PreservesFirstErrorAndSkipsRemainingSteps) {
    for (const auto failFirst : {true, false}) {
        std::vector<int> calls;
        auto result = pipeline(
            [&]() -> Result<void> {
                calls.push_back(1);
                if (failFirst) {
                    return Error::unexpected(
                        ErrorCode::invalidArgument, "first failed"
                    );
                }
                return {};
            },
            [&]() -> Result<void> {
                calls.push_back(2);
                return Error::unexpected(
                    ErrorCode::fileSystemError, "second failed"
                );
            },
            [&]() -> Result<void> {
                calls.push_back(3);
                return {};
            }
        );
        ASSERT_FALSE(result);
        EXPECT_EQ(
            result.error().code(),
            failFirst ? ErrorCode::invalidArgument : ErrorCode::fileSystemError
        );
        EXPECT_EQ(
            result.error().message(),
            failFirst ? "first failed" : "second failed"
        );
        EXPECT_EQ(
            calls, failFirst ? std::vector<int>{1} : std::vector<int>({1, 2})
        );
    }
}

TEST(Pipeline, AcceptsMoveOnlyRvaluesAndLvalueCallables) {
    int total = 0;
    auto step = [value = std::make_unique<int>(3), &total]() -> Result<void> {
        total += *value;
        return {};
    };
    auto result = pipeline(
        step, [value = std::make_unique<int>(4), &total]() -> Result<void> {
            total += *value;
            return {};
        }
    );
    EXPECT_TRUE(result);
    EXPECT_EQ(total, 7);
}

TEST(MoveOnlyFunction, EmptyAndResetFunctionsHaveNoCallable) {
    MoveOnlyFunction<int()> empty;
    EXPECT_FALSE(empty);
    MoveOnlyFunction<void()> function;
    function = [] {};
    EXPECT_TRUE(function);
    function = nullptr;
    EXPECT_FALSE(function);
}

TEST(MoveOnlyFunctionDeathTest, InvokingEmptyFunctionAborts) {
    EXPECT_DEATH((MoveOnlyFunction<void()>{}()), "");
}

TEST(MoveOnlyFunction, AcceptsLvalueCallables) {
    int calls = 0;
    auto callback = [&] { return ++calls; };
    MoveOnlyFunction<int()> function{callback};
    EXPECT_EQ(function(), 1);
    function = callback;
    EXPECT_EQ(function(), 2);
}

TEST(MoveOnlyFunction, MovesUniqueCapturesAndForwardsArgumentsAndResults) {
    MoveOnlyFunction<int(int)> function{
        [value = std::make_unique<int>(3)](int n) { return *value + n; }
    };
    auto moved = std::move(function);
    EXPECT_FALSE(function);
    EXPECT_EQ(moved(4), 7);
    function = std::move(moved);
    EXPECT_FALSE(moved);
    EXPECT_EQ(function(2), 5);
    MoveOnlyFunction<void(int&)> update{[](int& n) { ++n; }};
    int n = 0;
    update(n);
    EXPECT_EQ(n, 1);
    MoveOnlyFunction<int(std::unique_ptr<int>)> consume{
        [](std::unique_ptr<int> value) { return *value; }
    };
    EXPECT_EQ(consume(std::make_unique<int>(8)), 8);
}

TEST(Functional, DeferRunsOnScopeExitAndCanBeDismissed) {
    int calls = 0;
    {
        DEFER { ++calls; };
    }
    EXPECT_EQ(calls, 1);
    {
        NAMED_DEFER(cleanup) { ++calls; };
        cleanup.dismiss();
    }
    EXPECT_EQ(calls, 1);
    DeferCall noop;
}

TEST(Functional, LazyEvaluationRunsOnlyWhenConverted) {
    int calls = 0;
    auto lazy = LAZY_EVALUATE(++calls);
    EXPECT_EQ(calls, 0);
    int first = lazy;
    int second = lazy;
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
    auto text = lazyEvaluate([] { return "hello"; });
    Str result = text;
    EXPECT_EQ(result, "hello");
}

TEST(Functional, ConvertsFilteredRangesAndVisitsVariants) {
    std::vector<int> input{1, 2, 3, 4};
    auto output = input | std::views::filter([](int n) { return n % 2 == 0; }) |
                  toVector<int>();
    EXPECT_THAT(output, ElementsAre(2, 4));
    EXPECT_THAT(input | toVector<int>(), ElementsAre(1, 2, 3, 4));
    std::variant<int, Str> value{"test"};
    EXPECT_EQ(
        std::visit(
            Overloader{
                [](int n) { return n; },
                [](const Str& s) { return int(s.size()); }
            },
            value
        ),
        4
    );
}

}  // namespace anchor::test
