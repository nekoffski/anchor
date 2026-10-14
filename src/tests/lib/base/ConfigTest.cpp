#include "TestSupport.hh"
#include "base/Config.hh"
#include "base/LibconfigReader.hh"

namespace anchor::test {
using namespace testing;
using ConfigTest = TemporaryDirectory;

TEST_F(ConfigTest, ReaderRejectsMissingAndMalformedFiles) {
    auto missing = LibconfigReader::create(path("absent"));
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), ErrorCode::fileNotFound);
    ASSERT_TRUE(fs.writeFile(path("invalid.conf"), "not valid {{"));
    auto invalid = LibconfigReader::create(path("invalid.conf"));
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code(), ErrorCode::invalidConfig);
}

TEST_F(ConfigTest, ReaderUsesFilesystemExistenceSeam) {
    StrictMock<MockFileSystem> mock;
    EXPECT_CALL(mock, exists(_)).WillOnce(Return(false));
    auto result = LibconfigReader::create("ignored.conf", mock);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::fileNotFound);
}

TEST_F(ConfigTest, ReaderLooksUpScalarsPathsAndLists) {
    const auto file = path("reader.conf");
    ASSERT_TRUE(fs.writeFile(file, R"(
        text = "hello"; count = 42; enabled = true; path = "/tmp/data";
        items = ("one", "two"); empty = (); scalar = "wrong";
    )"));
    auto reader = LibconfigReader::create(file);
    ASSERT_TRUE(reader);
    EXPECT_EQ(*reader->lookup<Str>("text"), "hello");
    EXPECT_EQ(*reader->lookup<int>("count"), 42);
    EXPECT_EQ(*reader->lookup<bool>("enabled"), true);
    EXPECT_EQ(reader->lookup<Path>("path")->str(), "/tmp/data");
    EXPECT_THAT(
        *reader->lookup<std::vector<Str>>("items"), ElementsAre("one", "two")
    );
    auto paths = reader->lookup<std::vector<Path>>("items");
    ASSERT_TRUE(paths);
    ASSERT_EQ(paths->size(), 2);
    EXPECT_EQ((*paths)[0].str(), "one");
    EXPECT_EQ((*paths)[1].str(), "two");
    std::vector<Str> values{"old"};
    EXPECT_TRUE(reader->lookup("empty", values));
    EXPECT_TRUE(values.empty());
    EXPECT_FALSE(reader->lookup("scalar", values));
    auto missing = reader->lookup<int>("missing");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), ErrorCode::invalidConfig);
    EXPECT_EQ(missing.error().message(), "Missing field: missing");
    Path unchanged{"original"};
    EXPECT_FALSE(reader->lookup("missing", unchanged));
    EXPECT_EQ(unchanged.str(), "original");
}

TEST_F(
    ConfigTest, ReaderChecksUnsigned16BitBoundsWithoutChangingValueOnFailure
) {
    const auto file = path("bounds.conf");
    ASSERT_TRUE(fs.writeFile(
        file, "zero=0; max=65535; negative=-1; overflow=65536; text=\"bad\";"
    ));
    auto reader = LibconfigReader::create(file);
    ASSERT_TRUE(reader);
    u16 value = 99;
    EXPECT_TRUE(reader->lookup("zero", value));
    EXPECT_EQ(value, 0);
    EXPECT_TRUE(reader->lookup("max", value));
    EXPECT_EQ(value, 65535);
    for (const auto* key : {"negative", "overflow", "missing", "text"}) {
        EXPECT_FALSE(reader->lookup(key, value));
        EXPECT_EQ(value, 65535);
    }
    u32 wide = 99;
    EXPECT_TRUE(reader->lookup("overflow", wide));
    EXPECT_EQ(wide, 65536);
    EXPECT_FALSE(reader->lookup("missing", wide));
    EXPECT_EQ(wide, 65536);
}

TEST_F(ConfigTest, ReaderChecksUnsigned32BitBoundsWithoutWrapping) {
    ASSERT_TRUE(fs.writeFile(
        path("bounds.conf"),
        "zero=0; max=4294967295L; negative=-1; overflow=4294967296L; "
        "text=\"bad\";"
    ));
    auto reader = LibconfigReader::create(path("bounds.conf"));
    ASSERT_TRUE(reader);
    u32 value = 99;
    EXPECT_TRUE(reader->lookup("zero", value));
    EXPECT_EQ(value, 0);
    EXPECT_TRUE(reader->lookup("max", value));
    EXPECT_EQ(value, maxOf<u32>());
    for (const auto* key : {"negative", "overflow", "missing", "text"}) {
        EXPECT_FALSE(reader->lookup(key, value));
        EXPECT_EQ(value, maxOf<u32>());
    }
}

TEST_F(ConfigTest, LoadsRequiredConfigurationFieldsAndKeepsDefaults) {
    const auto file = path("server.conf");
    ASSERT_TRUE(fs.writeFile(file, R"(
        api = { port = 1234; };
        paths = { workspace = "workspace"; backup_plans = "plans"; };
    )"));
    auto config = Config::load(file);
    ASSERT_TRUE(config);
    EXPECT_EQ(config->api.port, 1234);
    EXPECT_EQ(config->paths.workspace.str(), path("workspace").str());
    EXPECT_EQ(config->paths.backupPlans.str(), path("plans").str());
    EXPECT_EQ(config->log.level, log::Level::trace);
    EXPECT_EQ(Config{}.api.port, 4448);
}

TEST_F(
    ConfigTest, ShippedConfigurationParsesAndResolvesPathsFromConfigDirectory
) {
    const auto configDirectory = path("config");
    ASSERT_TRUE(fs.createDirectory(configDirectory));
    const auto configPath = Path::join(configDirectory, "server.conf");
    ASSERT_TRUE(fs.copyFile(
        Path::join(ANCHOR_TEST_SOURCE_DIR, "packaging/server.conf"), configPath
    ));
    auto config = Config::load(configPath);
    ASSERT_TRUE(config) << config.error().message();
    EXPECT_EQ(config->api.port, 4448);
    EXPECT_EQ(
        std::filesystem::path(config->paths.workspace.str()).lexically_normal(),
        std::filesystem::path(path("workspace").str())
    );
    EXPECT_EQ(config->paths.backupPlans.str(), path("config/plans").str());
}

TEST_F(ConfigTest, AbsoluteAndEnvironmentPathsAreNotPrefixedByConfigDirectory) {
    ScopedEnv environment{"ANCHOR_TEST_CONFIG", root.str()};
    ASSERT_TRUE(fs.writeFile(path("server.conf"), R"(
        api={port=4448;};
        paths={workspace="$ANCHOR_TEST_CONFIG/work"; backup_plans="/absolute/plans";};
    )"));
    auto config = Config::load(path("server.conf"));
    ASSERT_TRUE(config);
    EXPECT_EQ(config->paths.workspace.str(), path("work").str());
    EXPECT_EQ(config->paths.backupPlans.str(), "/absolute/plans");
}

TEST_F(ConfigTest, ConfigReportsEachMissingRequiredFieldAndInvalidPorts) {
    const std::vector<std::pair<Str, Str>> cases{
        {"", "api.port"},
        {"api={port=4448;};", "paths.workspace"},
        {"api={port=4448;}; paths={workspace=\"work\";};",
         "paths.backup_plans"},
        {"api={port=-1;};", "api.port"},
        {"api={port=65536;};", "api.port"},
    };
    for (const auto& [content, key] : cases) {
        SCOPED_TRACE(content);
        ASSERT_TRUE(fs.writeFile(path("server.conf"), content));
        auto config = Config::load(path("server.conf"));
        ASSERT_FALSE(config);
        EXPECT_EQ(config.error().code(), ErrorCode::invalidConfig);
        EXPECT_EQ(config.error().message(), "Missing field: " + key);
    }
    auto missing = Config::load(path("absent"));
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), ErrorCode::fileNotFound);
}

TEST_F(ConfigTest, InvalidListsReturnErrorsAndPreserveOutput) {
    ASSERT_TRUE(
        fs.writeFile(path("reader.conf"), "scalar=1; mixed=(\"one\",2);")
    );
    auto reader = LibconfigReader::create(path("reader.conf"));
    ASSERT_TRUE(reader);
    std::vector<Str> values{"old"};
    EXPECT_FALSE(reader->lookup("absent", values));
    EXPECT_FALSE(reader->lookup("mixed", values));
    EXPECT_THAT(values, ElementsAre("old"));
    auto missing = reader->lookup<std::vector<Str>>("absent");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), ErrorCode::invalidConfig);
}

}  // namespace anchor::test
