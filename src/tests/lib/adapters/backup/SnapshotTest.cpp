#include "TestSupport.hh"
#include "adapters/backup/DirectorySnapshotter.hh"
#include "adapters/backup/OnDiskSnapshotStore.hh"
#include "core/services/SourceTreeBuilder.hh"

namespace anchor::test {
using namespace testing;
using SnapshotTest = TemporaryDirectory;

TEST_F(SnapshotTest, CopiesNestedFilesAndDirectoryLeavesAcrossSources) {
    ASSERT_TRUE(fs.createDirectory(path("first/docs/nested")));
    ASSERT_TRUE(fs.createDirectory(path("first/assets/subdirectory")));
    ASSERT_TRUE(fs.createDirectory(path("first/empty")));
    ASSERT_TRUE(fs.createDirectory(path("second")));
    const std::vector<std::pair<Str, Str>> files{
        {"first/docs/one.txt", "one"},
        {"first/docs/nested/deep.txt", "deep"},
        {"first/assets/subdirectory/data.bin", "data"},
        {"first/assets/whole.bin", "whole directory"},
        {"second/two.txt", "two"},
    };
    for (const auto& [name, content] : files) {
        ASSERT_TRUE(fs.writeFile(path(name), content));
    }
    ASSERT_TRUE(fs.writeFile(path("first/docs/ignored.txt"), "excluded"));
    const std::vector<Source> sources{
        {path("first"),
         {"docs/one.txt", "docs/nested/deep.txt", "assets",
          "assets/subdirectory/data.bin", "empty"}},
        {path("second"), {"two.txt"}},
    };
    const auto trees = SourceTreeBuilder::build(fs, sources);
    DirectorySnapshotter snapshotter{fs};
    const Str name = "daily";
    const auto workspace = path("workspace");
    auto snapshot = snapshotter.snapshot({name, trees, workspace});
    ASSERT_TRUE(snapshot);
    EXPECT_EQ(snapshot->name, name);
    EXPECT_EQ(snapshot->location.type, LocationType::local);
    const auto snapshotPath = Path::join(workspace, snapshot->name);
    EXPECT_EQ(snapshot->location.path, snapshotPath.str());
    for (const auto& [relative, content] : std::vector<std::pair<Str, Str>>{
             {"docs/one.txt", "one"},
             {"docs/nested/deep.txt", "deep"},
             {"assets/subdirectory/data.bin", "data"},
             {"assets/whole.bin", "whole directory"},
             {"two.txt", "two"}
         }) {
        auto copied = fs.readFile(Path::join(snapshotPath, relative));
        ASSERT_TRUE(copied) << relative;
        EXPECT_EQ(*copied, content);
    }
    EXPECT_TRUE(fs.isDirectory(Path::join(snapshotPath, "empty")));
    EXPECT_FALSE(fs.exists(Path::join(snapshotPath, "docs/ignored.txt")));
    for (const auto& [name, content] : files) {
        auto original = fs.readFile(path(name));
        ASSERT_TRUE(original);
        EXPECT_EQ(*original, content);
    }
}

TEST(DirectorySnapshotter, EmptySourcesProduceAnEmptySnapshot) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    EXPECT_CALL(fs, createDirectory(_)).WillOnce(Return(Result<void>{}));
    const Str name = "empty";
    const Path workspace{"/workspace"};
    auto result = snapshotter.snapshot({name, {}, workspace});
    ASSERT_TRUE(result);
    EXPECT_EQ(result->name, name);
    EXPECT_EQ(result->location.path, Path::join(workspace, result->name).str());
}

TEST(DirectorySnapshotter, EmptySourceTreesAreSkipped) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    EXPECT_CALL(fs, createDirectory(_)).WillOnce(Return(Result<void>{}));
    const Str name = "empty";
    const Path workspace{"/workspace"};
    const std::vector<SourceTree> trees{{.root = "/missing", .nodes = {}}};
    EXPECT_TRUE(snapshotter.snapshot({name, trees, workspace}));
}

TEST(DirectorySnapshotter, CreationFailureStopsBeforeCopying) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Return(
            Error::unexpected(ErrorCode::fileSystemError, "mkdir failed")
        ));
    const Str name = "daily";
    const Path workspace{"/workspace"};
    const std::vector<SourceTree> trees{
        {.root = "/source",
         .nodes = {
             {"file",
              {.type = SourceTree::NodeType::file,
               .relativePath = "file",
               .name = "file",
               .children = {}}}
         }}
    };
    auto result = snapshotter.snapshot({name, trees, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::fileSystemError);
    EXPECT_EQ(result.error().message(), "mkdir failed");
}

TEST(DirectorySnapshotter, NestedDirectoryCreationFailureStopsBeforeCopying) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Return(Result<void>{}))
        .WillOnce(Return(Result<void>{}))
        .WillOnce(Invoke([](const Path& destination) -> Result<void> {
            EXPECT_EQ(destination.filenameWithExtension(), "docs");
            return Error::unexpected(
                ErrorCode::fileSystemError, "nested mkdir failed"
            );
        }));
    const Str name = "daily";
    const Path workspace{"/workspace"};
    const std::vector<SourceTree> trees{
        {.root = "/source",
         .nodes = {
             {"docs",
              {.type = SourceTree::NodeType::directory,
               .relativePath = "docs",
               .name = "docs",
               .children =
                   {{"file",
                     {.type = SourceTree::NodeType::file,
                      .relativePath = "docs/file",
                      .name = "file",
                      .children = {}}}}}}
         }}
    };
    auto result = snapshotter.snapshot({name, trees, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::fileSystemError);
    EXPECT_EQ(result.error().message(), "nested mkdir failed");
}

TEST(DirectorySnapshotter, CopyFailureStopsAndPreservesError) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    EXPECT_CALL(fs, createDirectory(_))
        .Times(2)
        .WillRepeatedly(Return(Result<void>{}));
    EXPECT_CALL(fs, copyRecursive(_, _))
        .WillOnce(Invoke(
            [](const Path& source, const Path& destination) -> Result<void> {
                EXPECT_EQ(source.str(), "/source/missing");
                EXPECT_EQ(destination.filenameWithExtension(), "missing");
                return Error::unexpected(
                    ErrorCode::fileNotFound, "missing source"
                );
            }
        ));
    const Str name = "daily";
    const Path workspace{"/workspace"};
    const std::vector<SourceTree> trees{
        {.root = "/source",
         .nodes =
             {{"missing",
               {.type = SourceTree::NodeType::file,
                .relativePath = "missing",
                .name = "missing",
                .children = {}}}}},
        {.root = "/other",
         .nodes = {
             {"never",
              {.type = SourceTree::NodeType::file,
               .relativePath = "never",
               .name = "never",
               .children = {}}}
         }}
    };
    auto result = snapshotter.snapshot({name, trees, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::fileNotFound);
    EXPECT_EQ(result.error().message(), "missing source");
}

TEST(DirectorySnapshotter, DirectoryCopyFailurePreservesError) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    EXPECT_CALL(fs, createDirectory(_))
        .Times(2)
        .WillRepeatedly(Return(Result<void>{}));
    EXPECT_CALL(fs, copyRecursive(_, _))
        .WillOnce(Invoke(
            [](const Path& source, const Path& destination) -> Result<void> {
                EXPECT_EQ(source.str(), "/source/assets");
                EXPECT_EQ(destination.filenameWithExtension(), "assets");
                return Error::unexpected(
                    ErrorCode::fileSystemError, "directory copy failed"
                );
            }
        ));
    const Str name = "daily";
    const Path workspace{"/workspace"};
    const std::vector<SourceTree> trees{
        {.root = "/source",
         .nodes = {
             {"assets",
              {.type = SourceTree::NodeType::directory,
               .relativePath = "assets",
               .name = "assets",
               .children = {}}}
         }}
    };
    auto result = snapshotter.snapshot({name, trees, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::fileSystemError);
    EXPECT_EQ(result.error().message(), "directory copy failed");
}

TEST(OnDiskSnapshotStore, CopiesResolvedPathsAndReturnsDestinationMetadata) {
    StrictMock<MockFileSystem> fs;
    OnDiskSnapshotStore store{fs};
    ScopedEnv environment{"ANCHOR_TEST_STORE", "/backup"};
    SnapshotMetadata metadata{
        "daily_123", Location::local("/workspace/daily_123")
    };
    auto destination = Location::local("$ANCHOR_TEST_STORE");
    EXPECT_CALL(fs, copyRecursive(_, _))
        .WillOnce(Invoke([](const Path& from, const Path& to) -> Result<void> {
            EXPECT_EQ(from.str(), "/workspace/daily_123");
            EXPECT_EQ(to.str(), "/backup/daily_123");
            return {};
        }));
    auto result = store.put(metadata, destination);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->name, metadata.name);
    EXPECT_EQ(result->location.str(), destination.str());
    EXPECT_EQ(metadata.location.path, "/workspace/daily_123");
}

TEST(OnDiskSnapshotStore, RejectsRemoteDestinationBeforeCopying) {
    StrictMock<MockFileSystem> fs;
    OnDiskSnapshotStore store{fs};
    auto result = store.put(
        {"snapshot", Location::local("/source")}, Location::s3("bucket")
    );
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::invalidArgument);
}

TEST(OnDiskSnapshotStore, CopyFailureReturnsFilesystemError) {
    StrictMock<MockFileSystem> fs;
    OnDiskSnapshotStore store{fs};
    EXPECT_CALL(fs, copyRecursive(_, _))
        .WillOnce(
            Return(Error::unexpected(ErrorCode::fileSystemError, "copy failed"))
        );
    auto result = store.put(
        {"snapshot", Location::local("/source")}, Location::local("/backup")
    );
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::fileSystemError);
    EXPECT_THAT(result.error().message(), HasSubstr("/backup/snapshot"));
}

TEST_F(SnapshotTest, DirectorySnapshotAndStorePreserveFileContents) {
    ASSERT_TRUE(fs.writeFile(path("source.txt"), "backup content"));
    const Path workspace = path("workspace");
    const Path destination = path("backups");
    ASSERT_TRUE(fs.createDirectory(workspace));
    ASSERT_TRUE(fs.createDirectory(destination));
    const Str name = "daily";
    const auto tree =
        SourceTreeBuilder::build(fs, Source{root, {"source.txt"}});
    DirectorySnapshotter snapshotter{fs};
    auto snapshot =
        snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
    ASSERT_TRUE(snapshot);
    OnDiskSnapshotStore store{fs};
    auto stored = store.put(*snapshot, Location::local(destination.str()));
    ASSERT_TRUE(stored);
    auto content = fs.readFile(
        Path::join(Path::join(destination, snapshot->name), "source.txt")
    );
    ASSERT_TRUE(content);
    EXPECT_EQ(*content, "backup content");
}

TEST_F(SnapshotTest, EmptyIncludeCopiesTheWholeSourceRoot) {
    ASSERT_TRUE(fs.createDirectory(path("source/nested")));
    ASSERT_TRUE(fs.writeFile(path("source/nested/file.txt"), "whole root"));
    DirectorySnapshotter snapshotter{fs};
    size_t index = 0;
    const auto workspace = path("workspace");
    for (const auto& includes : std::vector<std::vector<Str>>{
             {""}, {"", "nested/file.txt"}, {"nested/file.txt", ""}
         }) {
        SCOPED_TRACE(includes.front());
        const auto name = fmt::format("daily_{}", index++);
        const auto tree =
            SourceTreeBuilder::build(fs, Source{path("source"), includes});
        auto snapshot =
            snapshotter.snapshot({name, std::span{&tree, 1}, workspace});
        ASSERT_TRUE(snapshot);
        auto copied =
            fs.readFile(Path::join(snapshot->location.path, "nested/file.txt"));
        ASSERT_TRUE(copied);
        EXPECT_EQ(*copied, "whole root");
    }
}

}  // namespace anchor::test
