#include "TestSupport.hh"
#include "base/Id.hh"

namespace anchor::test {
using namespace testing;

TEST(IdPool, AllocatesSequentialIdsAndReusesReleasedIdsInOrder) {
    IdPool pool;
    EXPECT_EQ(pool.acquire(), 0);
    EXPECT_EQ(pool.acquire(), 1);
    EXPECT_EQ(pool.acquire(), 2);
    pool.release(1);
    pool.release(0);
    EXPECT_EQ(pool.acquire(), 1);
    EXPECT_EQ(pool.acquire(), 0);
    EXPECT_EQ(pool.acquire(), 3);
}

TEST(IdLake, SharesPoolsAcrossInstancesButSeparatesTypes) {
    struct First {};
    struct Second {};
    IdLake first, second;
    auto id = first.acquire<First>();
    EXPECT_EQ(second.acquire<First>(), id + 1);
    EXPECT_EQ(first.acquire<Second>(), 0);
    second.release<First>(id);
    EXPECT_EQ(first.acquire<First>(), id);
}

}  // namespace anchor::test
