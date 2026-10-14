#include "TestSupport.hh"
#include "adapters/backup/DirectorySnapshotter.hh"
#include "adapters/backup/OnDiskSnapshotStore.hh"

namespace anchor::test {
using namespace testing;

TEST(DirectorySnapshotter, CreatesNamedSnapshotAndCopiesResolvedFilters) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    ScopedEnv environment{"ANCHOR_TEST_SOURCE", "/source"};
    const Str name = "daily";
    const std::vector<Path> filters{
        "$ANCHOR_TEST_SOURCE/first.txt", "/other/second.txt"
    };
    const Path workspace{"/workspace"};
    Str created;
    InSequence sequence;
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([&](const Path& path) -> Result<Path> {
            created = path.str();
            EXPECT_THAT(created, StartsWith("/workspace/daily_"));
            return path;
        }));
    EXPECT_CALL(fs, copyFile(_, _))
        .WillOnce(Invoke([&](const Path& from, const Path& to) -> Result<void> {
            EXPECT_EQ(from.str(), "/source/first.txt");
            EXPECT_EQ(to.str(), created + "/first.txt");
            return {};
        }));
    EXPECT_CALL(fs, copyFile(_, _))
        .WillOnce(Invoke([&](const Path& from, const Path& to) -> Result<void> {
            EXPECT_EQ(from.str(), "/other/second.txt");
            EXPECT_EQ(to.str(), created + "/second.txt");
            return {};
        }));
    auto result = snapshotter.snapshot({name, filters, workspace});
    ASSERT_TRUE(result);
    EXPECT_EQ(result->location.type, LocationType::local);
    EXPECT_EQ(result->location.path, created);
    EXPECT_EQ(result->name, Path{created}.filenameWithExtension());
}

TEST(DirectorySnapshotter, EmptyFiltersProduceAnEmptySnapshot) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([](const Path& path) -> Result<Path> {
            return path;
        }));
    const Str name = "empty";
    const Path workspace{"/workspace"};
    auto result = snapshotter.snapshot({name, {}, workspace});
    ASSERT_TRUE(result);
    EXPECT_THAT(result->name, StartsWith("empty_"));
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
    const std::vector<Path> filters{"/source"};
    auto result = snapshotter.snapshot({name, filters, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().message(), "mkdir failed");
}

TEST(DirectorySnapshotter, CopyFailureStopsAndPreservesError) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Invoke([](const Path& path) -> Result<Path> {
            return path;
        }));
    EXPECT_CALL(fs, copyFile(_, _))
        .WillOnce(
            Return(Error::unexpected(ErrorCode::fileNotFound, "missing source"))
        );
    const Str name = "daily";
    const Path workspace{"/workspace"};
    const std::vector<Path> filters{"/missing", "/never-copied"};
    auto result = snapshotter.snapshot({name, filters, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::fileNotFound);
    EXPECT_EQ(result.error().message(), "missing source");
}

TEST(OnDiskSnapshotStore, CopiesResolvedPathsAndReturnsDestinationMetadata) {
    StrictMock<MockFileSystem> fs;
    OnDiskSnapshotStore store{fs};
    ScopedEnv environment{"ANCHOR_TEST_STORE", "/backup"};
    SnapshotMetadata metadata{
        "daily_123", Location::local("/workspace/daily_123")
    };
    auto destination = Location::local("$ANCHOR_TEST_STORE");
    EXPECT_CALL(fs, copyDirectoryRecursive(_, _))
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
    EXPECT_CALL(fs, copyDirectoryRecursive(_, _))
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

using SnapshotTest = TemporaryDirectory;

TEST_F(SnapshotTest, DirectorySnapshotAndStorePreserveFileContents) {
    ASSERT_TRUE(fs.writeFile(path("source.txt"), "backup content"));
    const Path workspace = path("workspace");
    const Path destination = path("backups");
    ASSERT_TRUE(fs.createDirectory(workspace));
    ASSERT_TRUE(fs.createDirectory(destination));
    const Str name = "daily";
    const std::vector<Path> filters{path("source.txt")};
    DirectorySnapshotter snapshotter{fs};
    auto snapshot = snapshotter.snapshot({name, filters, workspace});
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

}  // namespace anchor::test
