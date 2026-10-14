#include "TestSupport.hh"
#include "base/Random.hh"

namespace anchor::test {
using namespace testing;

TEST(Random, GeneratesValidDistinctUuidsAndStableObjectIdentity) {
    auto& engine = RandomEngine::get();
    auto first = engine.uuid();
    auto second = engine.uuid();
    EXPECT_TRUE(uuids::uuid::is_valid_uuid(first));
    EXPECT_TRUE(uuids::uuid::is_valid_uuid(second));
    EXPECT_NE(first, second);
    EXPECT_TRUE(uuids::uuid::is_valid_uuid(Tag<Str>::fromUuid().get()));
    WithUuid object;
    EXPECT_TRUE(uuids::uuid::is_valid_uuid(object.uuid()));
    EXPECT_EQ(&object.uuid(), &object.uuid());
    EXPECT_EQ(&engine, &RandomEngine::get());
}

}  // namespace anchor::test
