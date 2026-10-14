#include "TestSupport.hh"
#include "base/String.hh"

namespace anchor::test {
using namespace testing;

TEST(String, SplitPreservesEmptyFieldsAndSupportsMultipleCharacterSeparators) {
    EXPECT_THAT(split("/a//b/", "/"), ElementsAre("", "a", "", "b", ""));
    EXPECT_THAT(split("local://path", "://"), ElementsAre("local", "path"));
    EXPECT_THAT(split("abc", "/"), ElementsAre("abc"));
    EXPECT_THAT(split("", "/"), ElementsAre(""));
    EXPECT_THAT(split("abc", ""), ElementsAre("abc"));
}

TEST(String, JoinHandlesZeroOneAndManyParts) {
    EXPECT_EQ(join({}, "/"), "");
    EXPECT_EQ(join({"one"}, "/"), "one");
    EXPECT_EQ(join({"", "a", "", "b"}, "/"), "/a//b");
    EXPECT_EQ(join({"a", "b"}, ""), "ab");
}

TEST(String, BinaryAndHexFormattingPadsToTypeWidth) {
    EXPECT_EQ(toBinaryString(u8{5}), "00000101");
    EXPECT_EQ(toBinaryString(u16{1}), "0000000000000001");
    EXPECT_EQ(toHexString(u8{15}), "0f");
    EXPECT_EQ(toHexString(u32{255}), "000000ff");
    Bytes bytes{0, 0x0f, 0xff};
    EXPECT_EQ(toBinaryString(bytes), "000000000000111111111111");
    EXPECT_EQ(toHexString(bytes), "000fff");
    EXPECT_EQ(toBinaryString(Bytes{}), "");
    EXPECT_EQ(toHexString(Bytes{}), "");
    constexpr StringLiteral literal{"abc"};
    EXPECT_STREQ(literal.value, "abc");
}

TEST(String, HexDumpDistinguishesPrintableBytes) {
    EXPECT_EQ(
        hexDump(Bytes{0x1f, 0x20, 0x41, 0x7e, 0x7f, 0xff}),
        "1f 20 41 7e 7f ff | . A~.."
    );
    EXPECT_EQ(hexDump(Bytes{}), " | ");
}

}  // namespace anchor::test
