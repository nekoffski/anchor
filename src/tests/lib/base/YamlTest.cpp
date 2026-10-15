#include "TestSupport.hh"
#include "base/Yaml.hh"

namespace anchor::test {
using namespace testing;
using YamlTest = TemporaryDirectory;

TEST_F(YamlTest, RejectsMissingMalformedAndNonMappingFiles) {
    auto missing = loadYaml(path("absent"));
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), ErrorCode::fileNotFound);
    for (const auto* content :
         {"api: [unterminated", "", "null", "text", "[one, two]",
          "api: {}\n---\npaths: {}"}) {
        SCOPED_TRACE(content);
        ASSERT_TRUE(fs.writeFile(path("invalid.yaml"), content));
        auto invalid = loadYaml(path("invalid.yaml"));
        ASSERT_FALSE(invalid);
        EXPECT_EQ(invalid.error().code(), ErrorCode::invalidConfig);
    }
}

TEST_F(YamlTest, UsesFilesystemExistenceSeam) {
    StrictMock<MockFileSystem> mock;
    EXPECT_CALL(mock, exists(_)).WillOnce(Return(false));
    auto result = loadYaml("ignored.yaml", mock);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::fileNotFound);
}

TEST_F(YamlTest, UsesFilesystemReadSeamAndPropagatesErrors) {
    StrictMock<MockFileSystem> mock;
    EXPECT_CALL(mock, exists(_)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, readFile(_))
        .WillOnce(Return(Str{"api: {port: 4448}"}))
        .WillOnce(
            Return(Error::unexpected(ErrorCode::fileSystemError, "read failed"))
        );
    const auto file = loadYaml("ignored.yaml", mock);
    ASSERT_TRUE(file);
    EXPECT_EQ(*yamlField<u16>((*file)["api"], "port"), 4448);
    auto failed = loadYaml("ignored.yaml", mock);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code(), ErrorCode::fileSystemError);
    EXPECT_EQ(failed.error().message(), "read failed");
}

TEST_F(YamlTest, ExposesRawNestedMappingsAndSequences) {
    ASSERT_TRUE(fs.writeFile(path("nested.yaml"), R"(
        text: hello
        enabled: true
        sources:
          - root: /documents
            includes: [one, two]
          - root: /pictures
            includes: []
    )"));
    const auto file = loadYaml(path("nested.yaml"));
    ASSERT_TRUE(file);
    EXPECT_EQ(*yamlField<Str>(*file, "text"), "hello");
    EXPECT_TRUE(*yamlField<bool>(*file, "enabled"));
    const auto sources = (*file)["sources"];
    ASSERT_TRUE(sources.IsSequence());
    ASSERT_EQ(sources.size(), 2);
    EXPECT_EQ(*yamlField<Str>(sources[0], "root"), "/documents");
    EXPECT_EQ(sources[0]["includes"][0].Scalar(), "one");
    EXPECT_EQ(sources[0]["includes"][1].Scalar(), "two");
    EXPECT_EQ(sources[1]["includes"].size(), 0);
    for (const auto& source : sources) {
        ASSERT_TRUE(source.IsMap());
        EXPECT_TRUE(yamlField<Str>(source, "root"));
    }
}

TEST_F(YamlTest, FieldsReportMissingAndInvalidValuesWithoutThrowing) {
    ASSERT_TRUE(fs.writeFile(path("values.yaml"), R"(
        null_value: null
        mapping: {key: value}
        sequence: [one, two]
        text: hello
    )"));
    const auto file = loadYaml(path("values.yaml"));
    ASSERT_TRUE(file);
    for (const auto* key : {"absent", "null_value", "mapping", "sequence"}) {
        auto field = yamlField<Str>(*file, key);
        ASSERT_FALSE(field) << key;
        EXPECT_EQ(field.error().code(), ErrorCode::invalidConfig);
        EXPECT_EQ(
            field.error().message(), Str{"Missing or invalid field: "} + key
        );
        EXPECT_FALSE(yamlField<int>((*file)[key], "nested"));
    }
    EXPECT_FALSE(yamlField<int>((*file)["text"], "nested"));
    EXPECT_FALSE(yamlField<int>(*file, "text"));
}

TEST_F(YamlTest, FieldsCheckUnsignedBounds) {
    ASSERT_TRUE(fs.writeFile(path("bounds.yaml"), R"(
        zero: 0
        max16: 65535
        overflow16: 65536
        max32: 4294967295
        overflow32: 4294967296
        negative: -1
        text: bad
    )"));
    const auto file = loadYaml(path("bounds.yaml"));
    ASSERT_TRUE(file);
    EXPECT_EQ(*yamlField<u16>(*file, "zero"), 0);
    EXPECT_EQ(*yamlField<u16>(*file, "max16"), maxOf<u16>());
    EXPECT_FALSE(yamlField<u16>(*file, "overflow16"));
    EXPECT_EQ(*yamlField<u32>(*file, "zero"), 0);
    EXPECT_EQ(*yamlField<u32>(*file, "overflow16"), 65536);
    EXPECT_EQ(*yamlField<u32>(*file, "max32"), maxOf<u32>());
    EXPECT_FALSE(yamlField<u32>(*file, "overflow32"));
    for (const auto* key : {"negative", "text", "absent"}) {
        EXPECT_FALSE(yamlField<u16>(*file, key));
        EXPECT_FALSE(yamlField<u32>(*file, key));
    }
}

}  // namespace anchor::test
