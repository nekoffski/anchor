#include <kcore/Random.hh>

#include "ManifestAssertions.hh"
#include "TestSupport.hh"
#include "adapters/backup/ArchiveSnapshotter.hh"
#include "adapters/backup/DirectorySnapshotter.hh"
#include "adapters/backup/OnDiskSnapshotStore.hh"
#include "adapters/serialization/SnapshotJson.hh"
#include "core/services/SourceTreeBuilder.hh"

namespace anchor::test {
using namespace kc;
using namespace testing;

class SnapshotStoreTest : public TemporaryDirectory {
   protected:
    void SetUp() override {
        TemporaryDirectory::SetUp();
        ASSERT_TRUE(fs.createDirectory(destination()));
    }
    Path destination() const { return path("destination"); }
    SnapshotMetadata capture(
        ArchiveType format = ArchiveType::directory,
        const Str& backupName = "daily"
    ) {
        const auto uuid = RandomEngine::get().uuid();
        const auto workspace = path("workspace");
        EXPECT_TRUE(fs.createDirectory(workspace));
        EXPECT_TRUE(fs.writeFile(path("data"), "captured data"));
        const auto tree = SourceTreeBuilder::build(fs, Source{root, {"data"}});
        const SnapshotArgs args{
            uuid, backupName, std::span{&tree, 1}, workspace
        };
        auto snapshot = format == ArchiveType::directory
                            ? DirectorySnapshotter{fs}.snapshot(args)
                            : ArchiveSnapshotter{fs, format}.snapshot(args);
        EXPECT_TRUE(snapshot);
        return snapshot ? *snapshot : SnapshotMetadata{};
    }
};

TEST_F(SnapshotStoreTest, PublishesSiblingManifestAndPayloadForEveryFormat) {
    OnDiskSnapshotStore store{fs};
    for (const auto format :
         {ArchiveType::directory, ArchiveType::zip, ArchiveType::tgz}) {
        auto snapshot = capture(format);
        const auto stored = store.put(snapshot, Location::local(destination()));
        ASSERT_TRUE(stored) << stored.error().message();
        EXPECT_EQ(
            Path{stored->location.path}.parent().str(),
            Path::join(destination(), "daily").str()
        );
        EXPECT_TRUE(fs.exists(stored->location.path));
        EXPECT_TRUE(fs.exists(snapshotManifestPath(*stored)));
        EXPECT_EQ(
            *fs.readFile(snapshotManifestPath(*stored)),
            *fs.readFile(snapshotManifestPath(snapshot))
        );
        expectManifestResult(
            *stored, *fs.readFile(snapshotManifestPath(*stored))
        );
        EXPECT_FALSE(store.put(snapshot, Location::local(destination())));
    }
    auto listed = store.list(Location::local(destination()), "daily");
    ASSERT_TRUE(listed);
    ASSERT_EQ(listed->size(), 3);
    EXPECT_GE((*listed)[0].created, (*listed)[1].created);
}

TEST_F(SnapshotStoreTest, ListsAfterRestartAndDeletesPayloadBeforeManifest) {
    OnDiskSnapshotStore store{fs};
    const auto snapshot = capture();
    auto stored = store.put(snapshot, Location::local(destination()));
    ASSERT_TRUE(stored);
    OnDiskSnapshotStore reopened{fs};
    auto listed = reopened.list(Location::local(destination()), "daily");
    ASSERT_TRUE(listed);
    ASSERT_EQ(listed->size(), 1);
    EXPECT_EQ(listed->front().uuid, snapshot.manifest.uuid);
    EXPECT_EQ(listed->front().location.path, stored->location.path);
    EXPECT_TRUE(
        reopened.list(Location::local(destination()), "other")->empty()
    );
    EXPECT_FALSE(reopened.remove(
        Location::local(destination()), "other", snapshot.manifest.uuid
    ));
    ASSERT_TRUE(reopened.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
    EXPECT_FALSE(fs.exists(stored->location.path));
    EXPECT_FALSE(fs.exists(snapshotManifestPath(*stored)));
    EXPECT_FALSE(reopened.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
}

TEST_F(SnapshotStoreTest, MissingPayloadIsNotListedAndItsManifestCanBeRemoved) {
    OnDiskSnapshotStore store{fs};
    const auto snapshot = capture();
    auto stored = store.put(snapshot, Location::local(destination()));
    ASSERT_TRUE(stored);
    ASSERT_TRUE(fs.removeAll(stored->location.path));
    auto listed = store.list(Location::local(destination()), "daily");
    ASSERT_TRUE(listed);
    EXPECT_TRUE(listed->empty());
    EXPECT_TRUE(store.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
    EXPECT_FALSE(fs.exists(snapshotManifestPath(*stored)));
}

TEST_F(
    SnapshotStoreTest, FailedManifestPublicationNeverExposesAPartialSnapshot
) {
    struct FailingManifest : FileSystem {
        Result<void> writeFileAtomic(const Path&, const Str&) const override {
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "publish failed"
            );
        }
    } failing;
    const auto snapshot = capture(ArchiveType::zip);
    OnDiskSnapshotStore store{failing};
    EXPECT_FALSE(store.put(snapshot, Location::local(destination())));
    EXPECT_TRUE(store.list(Location::local(destination()), "daily")->empty());
}

TEST_F(SnapshotStoreTest, RemovalFailurePreservesTheManifestForRetry) {
    const auto snapshot = capture();
    OnDiskSnapshotStore store{fs};
    auto stored = store.put(snapshot, Location::local(destination()));
    ASSERT_TRUE(stored);
    struct FailingRemoval : FileSystem {
        Result<void> removeAll(const Path&) const override {
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "permission denied"
            );
        }
    } failing;
    OnDiskSnapshotStore broken{failing};
    EXPECT_FALSE(broken.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
    EXPECT_TRUE(fs.exists(stored->location.path));
    EXPECT_TRUE(fs.exists(snapshotManifestPath(*stored)));
    EXPECT_TRUE(store.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
}

TEST_F(
    SnapshotStoreTest, RejectsEscapingNamesTamperedManifestsAndSymlinkPayloads
) {
    OnDiskSnapshotStore store{fs};
    EXPECT_FALSE(store.list(Location::local(destination()), "../outside"));
    EXPECT_FALSE(store.remove(
        Location::local(destination()), "/absolute", RandomEngine::get().uuid()
    ));
    EXPECT_FALSE(store.list(Location::s3("bucket"), "daily"));
    const auto invalidDestination =
        Location::local(destination().str() + Str(1, '\0') + "/child");
    EXPECT_FALSE(store.list(invalidDestination, "daily"));
    auto snapshot = capture();
    ASSERT_TRUE(fs.writeFile(snapshotManifestPath(snapshot), "tampered"));
    EXPECT_FALSE(store.put(snapshot, Location::local(destination())));
    snapshot = capture();
    auto stored = store.put(snapshot, Location::local(destination()));
    ASSERT_TRUE(stored);
    ASSERT_TRUE(fs.removeAll(stored->location.path));
    ASSERT_TRUE(fs.writeFile(path("outside"), "keep me"));
    std::filesystem::create_symlink(
        path("outside").str(), stored->location.path
    );
    EXPECT_FALSE(store.list(Location::local(destination()), "daily"));
    EXPECT_FALSE(store.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
    EXPECT_EQ(*fs.readFile(path("outside")), "keep me");
}

TEST_F(SnapshotStoreTest, InvalidManifestReportsAnErrorInsteadOfHidingHistory) {
    OnDiskSnapshotStore store{fs};
    const auto snapshot = capture();
    auto stored = store.put(snapshot, Location::local(destination()));
    ASSERT_TRUE(stored);
    ASSERT_TRUE(fs.writeFile(snapshotManifestPath(*stored), "bad json"));
    EXPECT_FALSE(store.list(Location::local(destination()), "daily"));
    EXPECT_FALSE(store.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
    EXPECT_TRUE(fs.exists(stored->location.path));
}

TEST_F(
    SnapshotStoreTest, FailedManifestRemovalCanBeRetriedAfterPayloadDeletion
) {
    const auto snapshot = capture();
    OnDiskSnapshotStore store{fs};
    auto stored = store.put(snapshot, Location::local(destination()));
    ASSERT_TRUE(stored);
    struct FailingRemove : FileSystem {
        Result<void> remove(const Path&) const override {
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "manifest removal failed"
            );
        }
    } failing;
    OnDiskSnapshotStore broken{failing};
    EXPECT_FALSE(broken.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
    EXPECT_FALSE(fs.exists(stored->location.path));
    EXPECT_TRUE(fs.exists(snapshotManifestPath(*stored)));
    EXPECT_TRUE(store.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
}

TEST_F(SnapshotStoreTest, CopyFailurePublishesNeitherManifestNorPayload) {
    const auto snapshot = capture();
    struct FailingCopy : FileSystem {
        Result<void> copyRecursive(const Path&, const Path&) const override {
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "copy failed"
            );
        }
    } failing;
    OnDiskSnapshotStore store{failing};
    EXPECT_FALSE(store.put(snapshot, Location::local(destination())));
    EXPECT_TRUE(store.list(Location::local(destination()), "daily")->empty());
}

TEST_F(SnapshotStoreTest, ChangedManifestBytesFailDigestVerification) {
    const auto snapshot = capture();
    auto json = Json::parse(*fs.readFile(snapshotManifestPath(snapshot)));
    json["created_at_ns"] =
        snapshot.manifest.created.time_since_epoch().count() + 1;
    ASSERT_TRUE(
        fs.writeFile(snapshotManifestPath(snapshot), json.dump(2) + "\n")
    );
    OnDiskSnapshotStore store{fs};
    auto result = store.put(snapshot, Location::local(destination()));
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().message(), "Snapshot manifest hash mismatch");
    EXPECT_TRUE(store.list(Location::local(destination()), "daily")->empty());
}

TEST_F(SnapshotStoreTest, ManifestHeaderCodecValidatesTheSharedSchema) {
    const auto snapshot = capture(ArchiveType::zip);
    const auto bytes = *fs.readFile(snapshotManifestPath(snapshot));
    auto header = parseSnapshotManifestHeader(bytes);
    ASSERT_TRUE(header);
    EXPECT_EQ(header->uuid, snapshot.manifest.uuid);
    EXPECT_EQ(header->backupName, snapshot.manifest.backupName);
    EXPECT_EQ(header->format, ArchiveType::zip);
    EXPECT_EQ(header->created, snapshot.manifest.created);
    const auto original = Json::parse(bytes);
    EXPECT_EQ(original["version"], 1);
    for (const auto& [key, value] : std::vector<std::pair<Str, Json>>{
             {"version", 2},
             {"format", "unknown"},
             {"hash_algorithm", "other"},
             {"created_at_ns", std::numeric_limits<u64>::max()}
         }) {
        auto malformed = original;
        malformed[key] = value;
        EXPECT_FALSE(parseSnapshotManifestHeader(malformed.dump()));
    }
}

TEST_F(SnapshotStoreTest, MetadataErrorsAreNotTreatedAsMissingPayloads) {
    const auto snapshot = capture();
    OnDiskSnapshotStore store{fs};
    auto stored = store.put(snapshot, Location::local(destination()));
    ASSERT_TRUE(stored);
    struct FailingInfo : FileSystem {
        Str payload;
        Result<FileInfo> info(const Path& path) const override {
            if (path.str() == payload) {
                return Error::unexpected(
                    kc::ErrorCode::fileSystemError, "stat failed"
                );
            }
            return FileSystem::info(path);
        }
    } failing;
    failing.payload = stored->location.path;
    OnDiskSnapshotStore broken{failing};
    EXPECT_FALSE(broken.list(Location::local(destination()), "daily"));
    EXPECT_FALSE(broken.remove(
        Location::local(destination()), "daily", snapshot.manifest.uuid
    ));
    EXPECT_TRUE(fs.exists(stored->location.path));
    EXPECT_TRUE(fs.exists(snapshotManifestPath(*stored)));
}

TEST_F(SnapshotStoreTest, RenameFailureDoesNotPublishAManifest) {
    const auto snapshot = capture();
    struct FailingRename : FileSystem {
        Result<void> rename(const Path&, const Path&) const override {
            return Error::unexpected(
                kc::ErrorCode::fileSystemError, "rename failed"
            );
        }
    } failing;
    OnDiskSnapshotStore store{failing};
    auto result = store.put(snapshot, Location::local(destination()));
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().message(), "rename failed");
    EXPECT_TRUE(store.list(Location::local(destination()), "daily")->empty());
    EXPECT_TRUE(fs.listEntries(Path::join(destination(), "daily"))->empty());
}

}  // namespace anchor::test
