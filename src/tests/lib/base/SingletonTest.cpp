#include "TestSupport.hh"
#include "base/Singleton.hh"

namespace anchor::test {

TEST(Singleton, LazilyCreatesOneSharedInstance) {
    struct Instance : Singleton<Instance> {
        int value = 0;
    };
    auto& first = Instance::get();
    first.value = 42;
    EXPECT_EQ(&first, &Instance::get());
    EXPECT_EQ(Instance::get().value, 42);
    Instance::noop();
}

TEST(UniqueInstanceDeathTest, ExposesExistingInstanceAndRejectsSecondInstance) {
    struct Instance : UniqueInstance<Instance> {
        using UniqueInstance::instance;
    };
    Instance first;
    EXPECT_EQ(&Instance::instance(), &first);
    EXPECT_DEATH({ Instance second; }, "");
}

TEST(UniqueInstanceDeathTest, AccessBeforeConstructionAborts) {
    struct Instance : UniqueInstance<Instance> {
        using UniqueInstance::instance;
    };
    EXPECT_DEATH(Instance::instance(), "");
}

}  // namespace anchor::test
