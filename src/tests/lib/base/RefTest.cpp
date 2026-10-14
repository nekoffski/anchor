#include "TestSupport.hh"
#include "base/Ref.hh"

namespace anchor::test {
using namespace testing;

class IntRef : public Ref<int> {
   public:
    using Ref::data;
    using Ref::Ref;
};
TEST(Ref, CopiesShareDataAndMovesRetainOwnership) {
    IntRef first{5};
    {
        auto copy = first;
        copy.data() = 7;
        EXPECT_EQ(first.data(), 7);
        auto moved = std::move(copy);
        moved.data() = 9;
    }
    EXPECT_EQ(std::as_const(first).data(), 9);
}

TEST(Ref, DestroysDataOnlyAfterTheLastOwnerAndCanCopyMovedFromReferences) {
    struct Value {
        explicit Value(int& destructions) : destructions(destructions) {}
        ~Value() { ++destructions; }
        int& destructions;
    };
    int destructions = 0;
    {
        Ref<Value> first{destructions};
        {
            auto copy = first;
            auto moved = std::move(copy);
            auto empty = copy;
        }
        EXPECT_EQ(destructions, 0);
    }
    EXPECT_EQ(destructions, 1);
}

}  // namespace anchor::test
