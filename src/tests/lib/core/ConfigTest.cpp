#include "TestSupport.hh"
#include "core/Config.hh"
#include "core/Error.hh"

namespace anchor::test {
using namespace kc;
using namespace testing;
using ConfigTest = TemporaryDirectory;

TEST_F(ConfigTest, LoadsRequiredConfigurationFieldsAndKeepsDefaults) {
    const auto file = path("server.yaml");
    ASSERT_TRUE(fs.writeFile(file, R"(
        api:
          port: 1234
        paths:
          workspace: workspace
          backup_plans: plans
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
    const auto configPath = Path::join(configDirectory, "server.yaml");
    ASSERT_TRUE(fs.copyFile(
        Path::join(ANCHOR_TEST_SOURCE_DIR, "packaging/server.yaml"), configPath
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
    ASSERT_TRUE(fs.writeFile(path("server.yaml"), R"(
        api: {port: 4448}
        paths:
          workspace: $ANCHOR_TEST_CONFIG/work
          backup_plans: /absolute/plans
    )"));
    auto config = Config::load(path("server.yaml"));
    ASSERT_TRUE(config);
    EXPECT_EQ(config->paths.workspace.str(), path("work").str());
    EXPECT_EQ(config->paths.backupPlans.str(), "/absolute/plans");
}

TEST_F(ConfigTest, ConfigReportsEachMissingRequiredFieldAndInvalidPorts) {
    const std::vector<std::pair<Str, Str>> cases{
        {"{}", "port"},
        {"api: {port: 4448}", "workspace"},
        {"api: {port: 4448}\npaths: {workspace: work}", "backup_plans"},
        {"api: {port: -1}", "port"},
        {"api: {port: 65536}", "port"},
        {"api: {port: null}", "port"},
        {"api: [4448]", "port"},
        {"api: scalar", "port"},
        {"api: null", "port"},
        {"api: {port: 4448}\npaths: []", "workspace"},
        {"api: {port: 4448}\npaths: scalar", "workspace"},
        {"api: {port: 4448}\npaths: {workspace: null}", "workspace"},
    };
    for (const auto& [content, key] : cases) {
        SCOPED_TRACE(content);
        ASSERT_TRUE(fs.writeFile(path("server.yaml"), content));
        auto config = Config::load(path("server.yaml"));
        ASSERT_FALSE(config);
        EXPECT_EQ(config.error().code(), kc::ErrorCode::invalidConfig);
        EXPECT_EQ(config.error().message(), "Missing or invalid field: " + key);
    }
    auto missing = Config::load(path("absent"));
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), kc::ErrorCode::fileNotFound);
}

}  // namespace anchor::test
