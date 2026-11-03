#include <gtest/gtest.h>

#include <array>

#include "adapters/crypto/Sha256.hh"

namespace anchor::test {

TEST(Sha256, MatchesEmptyAndAbcReferenceDigests) {
    const auto empty = sha256({});
    ASSERT_TRUE(empty);
    EXPECT_EQ(
        *empty,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    );
    const std::array<Byte, 3> abc{'a', 'b', 'c'};
    const auto digest = sha256(abc);
    ASSERT_TRUE(digest);
    EXPECT_EQ(
        *digest,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    );
}

TEST(Sha256, StreamingAndMovingPreservePartialInput) {
    auto original = Sha256::create();
    ASSERT_TRUE(original);
    const std::array<Byte, 1> first{'a'};
    ASSERT_TRUE(original->update(first));
    auto moved = std::move(*original);
    EXPECT_FALSE(original->update(first));
    EXPECT_FALSE(original->finish());
    const std::array<Byte, 2> remaining{'b', 'c'};
    ASSERT_TRUE(moved.update(remaining));
    const auto digest = moved.finish();
    ASSERT_TRUE(digest);
    EXPECT_EQ(
        *digest,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    );
    EXPECT_FALSE(moved.update(remaining));
    EXPECT_FALSE(moved.finish());
}

}  // namespace anchor::test
