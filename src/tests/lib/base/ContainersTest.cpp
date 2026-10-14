#include "TestSupport.hh"
#include "base/FlatMap.hh"
#include "base/InterfaceMap.hh"

namespace anchor::test {
using namespace testing;

TEST(FlatMap, InsertsFindsAndMutatesValues) {
    FlatMap<Str, Str> map{2};
    EXPECT_FALSE(map.has("missing"));
    EXPECT_EQ(map.find("missing"), nullptr);
    auto& value = map.emplace("first", 3, 'a');
    EXPECT_EQ(value, "aaa");
    const Str second = "second";
    map.insert("second", second);
    map.insert("third", Str{"third"});
    EXPECT_TRUE(map.has("first"));
    *map.find("first") = "changed";
    EXPECT_EQ(*std::as_const(map).find("first"), "changed");
    EXPECT_EQ(std::as_const(map).find("absent"), nullptr);
}

TEST(FlatMap, IteratesMutableAndConstValuesWithOrWithoutKeys) {
    FlatMap<int, int> map;
    map.emplace(1, 10);
    map.emplace(2, 20);
    map.forEach([](const int& key, int& value) { value += key; });
    map.forEach([](int& value) { ++value; });
    std::vector<int> values;
    std::as_const(map).forEach([&](const int& value) {
        values.push_back(value);
    });
    EXPECT_THAT(values, ElementsAre(12, 23));
    std::vector<int> keys;
    std::as_const(map).forEach([&](const int& key, const int&) {
        keys.push_back(key);
    });
    EXPECT_THAT(keys, ElementsAre(1, 2));
}

TEST(FlatMap, RemovesAllMatchingRecordsAndSupportsBatchRemovalAndMoves) {
    FlatMap<int, int> map;
    map.emplace(1, 10);
    map.emplace(1, 11);
    EXPECT_EQ(*map.find(1), 10);
    map.emplace(2, 20);
    map.emplace(3, 30);
    map.remove(1);
    EXPECT_FALSE(map.has(1));
    map.remove(999);
    const std::vector<int> keys{2, 999};
    map.remove(keys);
    auto moved = std::move(map);
    EXPECT_TRUE(moved.has(3));
    EXPECT_FALSE(moved.has(2));
    FlatMap<int, int> target;
    target = std::move(moved);
    EXPECT_EQ(*target.find(3), 30);
}

TEST(FlatMap, OwnsMoveOnlyValues) {
    FlatMap<int, std::unique_ptr<int>> map;
    map.emplace(1, std::make_unique<int>(42));
    EXPECT_EQ(**map.find(1), 42);
    map.remove(1);
    EXPECT_FALSE(map.has(1));
}

TEST(InterfaceMap, ConstructsAndOwnsRegisteredImplementations) {
    struct Interface {
        virtual ~Interface() = default;
        virtual int value() = 0;
    };
    struct Implementation : Interface {
        explicit Implementation(int n) : n(n) {}
        int value() override { return n; }
        int n;
    };
    InterfaceMap<int, Interface> map;
    EXPECT_EQ(map.get(0), nullptr);
    map.add<Implementation>(1, 42);
    EXPECT_EQ(map.get(1)->value(), 42);
    auto owned = std::make_unique<Implementation>(7);
    auto* pointer = owned.get();
    map.add(2, std::move(owned));
    EXPECT_EQ(map.get(2), pointer);
}

}  // namespace anchor::test
