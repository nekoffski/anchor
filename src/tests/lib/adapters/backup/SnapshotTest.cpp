#include <kcore/Random.hh>
#include <kcore/serialization/Json.hh>

#include "ManifestAssertions.hh"
#include "TestSupport.hh"
#include "adapters/backup/ArchiveSnapshotter.hh"
#include "adapters/backup/DirectorySnapshotter.hh"
#include "adapters/backup/OnDiskSnapshotStore.hh"
#include "core/Error.hh"
#include "core/services/SourceTreeBuilder.hh"

namespace anchor::test {
using namespace kc;
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
    const Uuid uuid = RandomEngine::get().uuid();
    const auto workspace = path("workspace");
    auto snapshot = snapshotter.snapshot({uuid, "daily", trees, workspace});
    ASSERT_TRUE(snapshot);
    EXPECT_EQ(snapshot->name, uuid);
    EXPECT_EQ(snapshot->location.type, LocationType::local);
    const auto snapshotPath = Path::join(workspace, snapshot->name);
    EXPECT_EQ(snapshot->location.path, snapshotPath.str());
    expectManifestResult(
        *snapshot, *fs.readFile(snapshotManifestPath(*snapshot))
    );
    for (const auto& [relative, content] : std::vector<std::pair<Str, Str>>{
             {"0/docs/one.txt", "one"},
             {"0/docs/nested/deep.txt", "deep"},
             {"0/assets/subdirectory/data.bin", "data"},
             {"0/assets/whole.bin", "whole directory"},
             {"1/two.txt", "two"}
         }) {
        auto copied = fs.readFile(Path::join(snapshotPath, relative));
        ASSERT_TRUE(copied) << relative;
        EXPECT_EQ(*copied, content);
    }
    EXPECT_TRUE(fs.isDirectory(Path::join(snapshotPath, "0/empty")));
    EXPECT_FALSE(fs.exists(Path::join(snapshotPath, "0/docs/ignored.txt")));
    for (const auto& [name, content] : files) {
        auto original = fs.readFile(path(name));
        ASSERT_TRUE(original);
        EXPECT_EQ(*original, content);
    }
}

TEST_F(SnapshotTest, EmptySourcesProduceAManifest) {
    DirectorySnapshotter snapshotter{fs};
    const Uuid uuid = RandomEngine::get().uuid();
    const auto workspace = path("workspace");
    auto result = snapshotter.snapshot({uuid, "daily", {}, workspace});
    ASSERT_TRUE(result);
    auto json = fs.readFile(snapshotManifestPath(*result));
    ASSERT_TRUE(json);
    expectManifestResult(*result, *json);
    auto manifest = Json::parse(*json);
    EXPECT_EQ(manifest["version"], SnapshotManifest::currentVersion);
    EXPECT_EQ(manifest["uuid"], uuid);
    EXPECT_TRUE(manifest["sources"].empty());
}

TEST_F(SnapshotTest, EmptySourcesKeepTheirIndicesAndOriginalRoots) {
    ASSERT_TRUE(fs.createDirectory(path("source")));
    ASSERT_TRUE(fs.writeFile(path("source/file"), "abc"));
    auto selected =
        SourceTreeBuilder::build(fs, Source{path("source"), {"file"}});
    const std::vector<SourceTree> trees{{.root = path("missing")}, selected};
    DirectorySnapshotter snapshotter{fs};
    const Uuid uuid = RandomEngine::get().uuid();
    const auto workspace = path("workspace");
    auto result = snapshotter.snapshot({uuid, "daily", trees, workspace});
    ASSERT_TRUE(result);
    EXPECT_TRUE(fs.isDirectory(Path::join(result->location.path, "0")));
    EXPECT_EQ(*fs.readFile(Path::join(result->location.path, "1/file")), "abc");
    auto manifest = Json::parse(*fs.readFile(snapshotManifestPath(*result)));
    ASSERT_EQ(manifest["sources"].size(), 2);
    EXPECT_EQ(manifest["sources"][0]["root"], path("missing").str());
    EXPECT_TRUE(manifest["sources"][0]["entries"].empty());
    const auto& file = manifest["sources"][1]["entries"][0];
    EXPECT_EQ(file["path"], "file");
    EXPECT_EQ(file["size"], 3);
    EXPECT_EQ(
        file["sha256"],
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    );
}

TEST(DirectorySnapshotter, CreationFailureStopsBeforeCopying) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter snapshotter{fs};
    EXPECT_CALL(fs, exists(_)).WillOnce(Return(false));
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Return(
            Error::unexpected(kc::ErrorCode::fileSystemError, "mkdir failed")
        ));
    const Uuid uuid = RandomEngine::get().uuid();
    const Path workspace{"/workspace"};
    auto result = snapshotter.snapshot({uuid, "daily", {}, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code(), kc::ErrorCode::fileSystemError);
    EXPECT_EQ(result.error().message(), "mkdir failed");
}

TEST_F(SnapshotTest, NestedDirectoryCreationFailurePreservesError) {
    struct FailingDirectories : FileSystem {
        Result<void> createDirectory(const Path& path) const override {
            if (path.filenameWithExtension() == "docs") {
                return Error::unexpected(
                    kc::ErrorCode::fileSystemError, "nested mkdir failed"
                );
            }
            return FileSystem::createDirectory(path);
        }
    } failing;
    ASSERT_TRUE(fs.createDirectory(path("source/docs")));
    ASSERT_TRUE(fs.writeFile(path("source/docs/file"), "data"));
    auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"docs/file"}});
    DirectorySnapshotter snapshotter{failing};
    const Uuid uuid = RandomEngine::get().uuid();
    const auto workspace = path("workspace");
    auto result =
        snapshotter.snapshot({uuid, "daily", std::span{&tree, 1}, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().message(), "nested mkdir failed");
}

TEST_F(SnapshotTest, InputFailureDoesNotProduceACompleteManifest) {
    struct FailingInput : FileSystem {
        Result<std::unique_ptr<File>> openFile(
            const Path& path, FileMode mode
        ) const override {
            if (mode == FileMode::read) {
                return Error::unexpected(
                    kc::ErrorCode::fileSystemError, "input unavailable"
                );
            }
            return FileSystem::openFile(path, mode);
        }
    } failing;
    ASSERT_TRUE(fs.createDirectory(path("source")));
    ASSERT_TRUE(fs.writeFile(path("source/file"), "data"));
    auto tree = SourceTreeBuilder::build(fs, Source{path("source"), {"file"}});
    DirectorySnapshotter snapshotter{failing};
    const Uuid uuid = RandomEngine::get().uuid();
    const auto workspace = path("workspace");
    auto result =
        snapshotter.snapshot({uuid, "daily", std::span{&tree, 1}, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().message(), "input unavailable");
    EXPECT_FALSE(fs.exists(Path::join(workspace, uuid + ".json")));
}

TEST_F(SnapshotTest, DirectoryListingFailurePreservesError) {
    struct FailingListing : FileSystem {
        Result<std::vector<Path>> listEntries(const Path&) const override {
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "listing failed"
            );
        }
    } failing;
    ASSERT_TRUE(fs.createDirectory(path("source/assets")));
    auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"assets"}});
    DirectorySnapshotter snapshotter{failing};
    const Uuid uuid = RandomEngine::get().uuid();
    const auto workspace = path("workspace");
    auto result =
        snapshotter.snapshot({uuid, "daily", std::span{&tree, 1}, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().message(), "listing failed");
}

TEST_F(
    SnapshotTest, ManifestHashesCapturedBytesWhenTheSourceChangesAfterReading
) {
    struct ChangingFile : File {
        ChangingFile(std::unique_ptr<File> file, Path path)
            : file(std::move(file)), path(std::move(path)) {}
        Result<size_t> read(std::span<Byte> data) override {
            return file->read(data);
        }
        Result<void> write(std::span<const Byte> data) override {
            return file->write(data);
        }
        Result<void> close() override {
            if (auto r = file->close(); not r) {
                return r;
            }
            return FileSystem{}.writeFile(path, "changed after reading");
        }
        std::unique_ptr<File> file;
        Path path;
    };
    struct ChangingSource : FileSystem {
        mutable size_t reads{};
        Result<std::unique_ptr<File>> openFile(
            const Path& path, FileMode mode
        ) const override {
            auto file = FileSystem::openFile(path, mode);
            if (not file || mode != FileMode::read) {
                return file;
            }
            ++reads;
            return std::make_unique<ChangingFile>(std::move(*file), path);
        }
    } changing;
    ASSERT_TRUE(fs.createDirectory(path("source")));
    ASSERT_TRUE(fs.writeFile(path("source/file"), "abc"));
    const auto tree =
        SourceTreeBuilder::build(fs, Source{path("source"), {"file"}});
    DirectorySnapshotter snapshotter{changing};
    const Uuid uuid = RandomEngine::get().uuid();
    const auto workspace = path("workspace");
    auto result =
        snapshotter.snapshot({uuid, "daily", std::span{&tree, 1}, workspace});
    ASSERT_TRUE(result);
    EXPECT_EQ(changing.reads, 1);
    EXPECT_EQ(*fs.readFile(path("source/file")), "changed after reading");
    EXPECT_EQ(*fs.readFile(Path::join(result->location.path, "0/file")), "abc");
    auto manifest = Json::parse(*fs.readFile(snapshotManifestPath(*result)));
    EXPECT_EQ(
        manifest["sources"][0]["entries"][0]["sha256"],
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    );
}

TEST_F(SnapshotTest, ExistingDirectorySnapshotIsNotOverwritten) {
    ASSERT_TRUE(fs.createDirectory(
        path("workspace/12345678-1234-4234-8234-123456789abc")
    ));
    ASSERT_TRUE(fs.writeFile(
        path("workspace/12345678-1234-4234-8234-123456789abc/existing.txt"),
        "existing"
    ));
    DirectorySnapshotter snapshotter{fs};
    const Uuid uuid = "12345678-1234-4234-8234-123456789abc";
    const auto workspace = path("workspace");
    EXPECT_FALSE(snapshotter.snapshot({uuid, "daily", {}, workspace}));
    EXPECT_EQ(
        *fs.readFile(
            path("workspace/12345678-1234-4234-8234-123456789abc/existing.txt")
        ),
        "existing"
    );
}

TEST_F(SnapshotTest, ManifestPublicationFailureFailsSnapshotCreation) {
    struct FailingManifest : FileSystem {
        Result<void> writeFileAtomic(const Path&, const Str&) const override {
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "manifest publication failed"
            );
        }
    } failing;
    DirectorySnapshotter snapshotter{failing};
    const Uuid uuid = RandomEngine::get().uuid();
    const auto workspace = path("workspace");
    auto result = snapshotter.snapshot({uuid, "daily", {}, workspace});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().message(), "manifest publication failed");
    EXPECT_FALSE(fs.exists(Path::join(workspace, uuid + ".json")));
}

TEST_F(SnapshotTest, DirectorySnapshotAndStorePreserveFileContents) {
    ASSERT_TRUE(fs.writeFile(path("source.txt"), "backup content"));
    const Path workspace = path("workspace");
    const Path destination = path("backups");
    ASSERT_TRUE(fs.createDirectory(workspace));
    ASSERT_TRUE(fs.createDirectory(destination));
    const Uuid uuid = RandomEngine::get().uuid();
    const auto tree =
        SourceTreeBuilder::build(fs, Source{root, {"source.txt"}});
    DirectorySnapshotter snapshotter{fs};
    auto snapshot =
        snapshotter.snapshot({uuid, "daily", std::span{&tree, 1}, workspace});
    ASSERT_TRUE(snapshot);
    const auto originalInfo = fs.info(path("source.txt"));
    const auto capturedInfo =
        fs.info(Path::join(snapshot->location.path, "0/source.txt"));
    ASSERT_TRUE(originalInfo);
    ASSERT_TRUE(capturedInfo);
    EXPECT_EQ(capturedInfo->permissions, originalInfo->permissions);
    EXPECT_EQ(capturedInfo->modified, originalInfo->modified);
    OnDiskSnapshotStore store{fs};
    auto stored = store.put(*snapshot, Location::local(destination.str()));
    ASSERT_TRUE(stored);
    auto content =
        fs.readFile(Path::join(stored->location.path, "0/source.txt"));
    ASSERT_TRUE(content);
    EXPECT_EQ(*content, "backup content");
    const auto manifestPath = snapshotManifestPath(*stored);
    expectManifestResult(*stored, *fs.readFile(manifestPath));
    EXPECT_EQ(stored->manifestSha256, snapshot->manifestSha256);
    EXPECT_EQ(stored->manifest.uuid, snapshot->manifest.uuid);
}

TEST_F(SnapshotTest, EmptyIncludeCopiesTheWholeSourceRoot) {
    ASSERT_TRUE(fs.createDirectory(path("source/nested")));
    ASSERT_TRUE(fs.writeFile(path("source/nested/file.txt"), "whole root"));
    DirectorySnapshotter snapshotter{fs};
    const auto workspace = path("workspace");
    for (const auto& includes : std::vector<std::vector<Str>>{
             {""}, {"", "nested/file.txt"}, {"nested/file.txt", ""}
         }) {
        SCOPED_TRACE(includes.front());
        const Uuid uuid = RandomEngine::get().uuid();
        const auto tree =
            SourceTreeBuilder::build(fs, Source{path("source"), includes});
        auto snapshot = snapshotter.snapshot(
            {uuid, "daily", std::span{&tree, 1}, workspace}
        );
        ASSERT_TRUE(snapshot);
        auto copied = fs.readFile(
            Path::join(snapshot->location.path, "0/nested/file.txt")
        );
        ASSERT_TRUE(copied);
        EXPECT_EQ(*copied, "whole root");
    }
}

TEST(SnapshotArgs, InvalidIdentitiesAreRejectedBeforeFilesystemAccess) {
    StrictMock<MockFileSystem> fs;
    DirectorySnapshotter directory{fs};
    ArchiveSnapshotter archive{fs, ArchiveType::zip};
    const Path workspace{"/unused"};
    const std::vector<SnapshotArgs> invalid{
        {"../escape", "daily", {}, workspace},
        {"12345678-1234-4234-8234-123456789abc", "", {}, workspace},
    };
    for (const auto& args : invalid) {
        EXPECT_FALSE(directory.snapshot(args));
        EXPECT_FALSE(archive.snapshot(args));
    }
}

}  // namespace anchor::test
