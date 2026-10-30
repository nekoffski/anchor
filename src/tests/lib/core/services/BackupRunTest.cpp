#include "TestSupport.hh"
#include "core/Error.hh"
#include "core/services/BackupRun.hh"

namespace anchor::test {
using namespace kc;
using namespace testing;

class BackupRunTest : public Test {
   protected:
    StrictMock<MockFileSystem> fs;
    StrictMock<MockBackupRepo> backups;
    StrictMock<MockSnapshotter> snapshotter;
    StrictMock<MockSnapshotStore> store;
    BackupPlan backup = plan();
    Path workspace{"/workspace"};
    SnapshotMetadata metadata{
        "snapshot", Location::local("/workspace/snapshot")
    };
    Str created;
    Uuid runUuid;

    void expectWorkspace() {
        EXPECT_CALL(fs, createDirectory(_))
            .WillOnce(Invoke([&](const Path& path) -> Result<void> {
                created = path.str();
                EXPECT_EQ(path.parent().str(), workspace.str());
                EXPECT_TRUE(uuids::uuid::is_valid_uuid(path.filename()));
                return {};
            }));
    }
    void expectSourceTree() {
        EXPECT_CALL(fs, glob("/source/file.txt"))
            .WillOnce(Return(std::vector<Path>{"/source/file.txt"}));
        EXPECT_CALL(fs, isDirectory(_)).WillOnce(Return(false));
    }
    void expectState(BackupRunState state) {
        EXPECT_CALL(
            backups,
            updateRunStatus(
                "daily", Truly([&, state](const BackupRunStatus& status) {
                    return status.state == state and status.uuid == runUuid and
                           not status.error;
                })
            )
        );
    }
    void expectCleanup(bool fail = false) {
        EXPECT_CALL(fs, removeAll(_))
            .WillOnce(Invoke([&, fail](const Path& path) -> Result<void> {
                EXPECT_EQ(path.str(), created);
                if (fail) {
                    return Error::unexpected(
                        kc::ErrorCode::fileSystemError, "cleanup failed"
                    );
                }
                return {};
            }));
    }
    void start() {
        BackupRun run{{backup, backups, snapshotter, store, fs, workspace}};
        runUuid = run.uuid();
        run.start();
    }
};

TEST_F(BackupRunTest, SuccessTransitionsInOrderAndCleansWorkspace) {
    InSequence sequence;
    expectState(BackupRunState::starting);
    expectWorkspace();
    expectState(BackupRunState::snapshotting);
    expectSourceTree();
    EXPECT_CALL(snapshotter, snapshot(_))
        .WillOnce(
            Invoke([&](const SnapshotArgs& args) -> Result<SnapshotMetadata> {
                EXPECT_THAT(args.name, StartsWith(backup.name + "_"));
                EXPECT_EQ(args.trees.size(), 1);
                if (args.trees.size() == 1) {
                    const auto& tree = args.trees.front();
                    EXPECT_EQ(tree.root.str(), "/source");
                    EXPECT_EQ(tree.nodes.size(), 1);
                    EXPECT_TRUE(tree.nodes.contains("file.txt"));
                    if (tree.nodes.contains("file.txt")) {
                        const auto& file = tree.nodes.at("file.txt");
                        EXPECT_EQ(file.name, "file.txt");
                        EXPECT_EQ(file.relativePath, "file.txt");
                        EXPECT_EQ(file.type, SourceTree::NodeType::file);
                        EXPECT_TRUE(file.children.empty());
                    }
                }
                EXPECT_EQ(args.workspace.str(), created);
                return metadata;
            })
        );
    expectState(BackupRunState::uploading);
    EXPECT_CALL(store, put(_, _))
        .WillOnce(Invoke(
            [&](const SnapshotMetadata& snapshot,
                const Location& destination) -> Result<SnapshotMetadata> {
                EXPECT_EQ(snapshot.name, metadata.name);
                EXPECT_EQ(destination.str(), backup.destination.str());
                return metadata;
            }
        ));
    EXPECT_CALL(
        backups,
        updateRunStatus("daily", Truly([&](const BackupRunStatus& status) {
                            return status.state == BackupRunState::completed and
                                   not status.error and status.uuid == runUuid;
                        }))
    );
    expectCleanup();
    start();
}

TEST_F(BackupRunTest, WorkspaceFailureDoesNotSnapshotOrRemoveWorkspace) {
    InSequence sequence;
    expectState(BackupRunState::starting);
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Return(
            Error::unexpected(kc::ErrorCode::fileSystemError, "mkdir failed")
        ));
    EXPECT_CALL(
        backups,
        updateRunStatus("daily", Truly([&](const BackupRunStatus& status) {
                            return status.uuid == runUuid &&
                                   status.state == BackupRunState::failed &&
                                   status.error &&
                                   status.error->code() ==
                                       kc::ErrorCode::fileSystemError;
                        }))
    );
    start();
}

TEST_F(BackupRunTest, SnapshotFailureIsStoredAndWorkspaceIsCleaned) {
    InSequence sequence;
    expectState(BackupRunState::starting);
    expectWorkspace();
    expectState(BackupRunState::snapshotting);
    expectSourceTree();
    EXPECT_CALL(snapshotter, snapshot(_))
        .WillOnce(Return(
            Error::unexpected(kc::ErrorCode::fileNotFound, "source missing")
        ));
    EXPECT_CALL(
        backups,
        updateRunStatus("daily", Truly([&](const BackupRunStatus& status) {
                            return status.uuid == runUuid &&
                                   status.state == BackupRunState::failed &&
                                   status.error &&
                                   status.error->code() ==
                                       kc::ErrorCode::fileNotFound &&
                                   status.error->message() == "source missing";
                        }))
    );
    expectCleanup();
    start();
}

TEST_F(BackupRunTest, StoreFailureIsStoredAndWorkspaceIsCleaned) {
    InSequence sequence;
    expectState(BackupRunState::starting);
    expectWorkspace();
    expectState(BackupRunState::snapshotting);
    expectSourceTree();
    EXPECT_CALL(snapshotter, snapshot(_)).WillOnce(Return(metadata));
    expectState(BackupRunState::uploading);
    EXPECT_CALL(store, put(_, _))
        .WillOnce(
            Return(Error::unexpected(ErrorCode::connectionFailure, "offline"))
        );
    EXPECT_CALL(
        backups,
        updateRunStatus("daily", Truly([&](const BackupRunStatus& status) {
                            return status.uuid == runUuid &&
                                   status.state == BackupRunState::failed &&
                                   status.error &&
                                   status.error->message() == "offline";
                        }))
    );
    expectCleanup();
    start();
}

TEST_F(BackupRunTest, CleanupFailureDoesNotReplaceSuccessfulBackupState) {
    InSequence sequence;
    expectState(BackupRunState::starting);
    expectWorkspace();
    expectState(BackupRunState::snapshotting);
    expectSourceTree();
    EXPECT_CALL(snapshotter, snapshot(_)).WillOnce(Return(metadata));
    expectState(BackupRunState::uploading);
    EXPECT_CALL(store, put(_, _)).WillOnce(Return(metadata));
    expectState(BackupRunState::completed);
    expectCleanup(true);
    start();
}

}  // namespace anchor::test
