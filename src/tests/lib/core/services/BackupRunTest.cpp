#include "TestSupport.hh"
#include "core/services/BackupRun.hh"

namespace anchor::test {
using namespace testing;

class BackupRunTest : public Test {
   protected:
    StrictMock<MockFileSystem> fs;
    StrictMock<MockRunRepo> runs;
    StrictMock<MockSnapshotter> snapshotter;
    StrictMock<MockSnapshotStore> store;
    BackupPlan backup = plan();
    Path workspace{"/workspace"};
    SnapshotMetadata metadata{
        "snapshot", Location::local("/workspace/snapshot")
    };
    Str created;

    void expectWorkspace() {
        EXPECT_CALL(fs, createDirectory(_))
            .WillOnce(Invoke([&](const Path& path) -> Result<Path> {
                created = path.str();
                EXPECT_EQ(path.parent().str(), workspace.str());
                EXPECT_TRUE(uuids::uuid::is_valid_uuid(path.filename()));
                return path;
            }));
    }
    void expectState(BackupRunState state) {
        EXPECT_CALL(runs, put("daily", Field(&BackupRunStatus::state, state)));
    }
    void expectCleanup(bool fail = false) {
        EXPECT_CALL(fs, removeAll(_))
            .WillOnce(Invoke([&, fail](const Path& path) -> Result<void> {
                EXPECT_EQ(path.str(), created);
                if (fail) {
                    return Error::unexpected(
                        ErrorCode::fileSystemError, "cleanup failed"
                    );
                }
                return {};
            }));
    }
    void start() {
        BackupRun{{backup, runs, snapshotter, store, fs, workspace}}.start();
    }
};

TEST_F(BackupRunTest, SuccessTransitionsInOrderAndCleansWorkspace) {
    InSequence sequence;
    expectWorkspace();
    expectState(BackupRunState::snapshotting);
    EXPECT_CALL(snapshotter, snapshot(_))
        .WillOnce(
            Invoke([&](const SnapshotArgs& args) -> Result<SnapshotMetadata> {
                EXPECT_EQ(args.name, backup.name);
                EXPECT_EQ(args.filters.size(), 1);
                if (args.filters.size() == 1) {
                    EXPECT_EQ(args.filters[0].str(), "/source");
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
        runs, put("daily",
                  AllOf(
                      Field(&BackupRunStatus::state, BackupRunState::completed),
                      Field(&BackupRunStatus::error, Eq(std::nullopt))
                  ))
    );
    expectCleanup();
    start();
}

TEST_F(BackupRunTest, WorkspaceFailureDoesNotSnapshotOrRemoveWorkspace) {
    InSequence sequence;
    EXPECT_CALL(fs, createDirectory(_))
        .WillOnce(Return(
            Error::unexpected(ErrorCode::fileSystemError, "mkdir failed")
        ));
    EXPECT_CALL(runs, put("daily", Truly([](const BackupRunStatus& status) {
                              return status.state == BackupRunState::failed &&
                                     status.error &&
                                     status.error->code() ==
                                         ErrorCode::fileSystemError;
                          })));
    start();
}

TEST_F(BackupRunTest, SnapshotFailureIsStoredAndWorkspaceIsCleaned) {
    InSequence sequence;
    expectWorkspace();
    expectState(BackupRunState::snapshotting);
    EXPECT_CALL(snapshotter, snapshot(_))
        .WillOnce(
            Return(Error::unexpected(ErrorCode::fileNotFound, "source missing"))
        );
    EXPECT_CALL(runs, put("daily", Truly([](const BackupRunStatus& status) {
                              return status.state == BackupRunState::failed &&
                                     status.error &&
                                     status.error->code() ==
                                         ErrorCode::fileNotFound &&
                                     status.error->message() ==
                                         "source missing";
                          })));
    expectCleanup();
    start();
}

TEST_F(BackupRunTest, StoreFailureIsStoredAndWorkspaceIsCleaned) {
    InSequence sequence;
    expectWorkspace();
    expectState(BackupRunState::snapshotting);
    EXPECT_CALL(snapshotter, snapshot(_)).WillOnce(Return(metadata));
    expectState(BackupRunState::uploading);
    EXPECT_CALL(store, put(_, _))
        .WillOnce(
            Return(Error::unexpected(ErrorCode::connectionFailure, "offline"))
        );
    EXPECT_CALL(runs, put("daily", Truly([](const BackupRunStatus& status) {
                              return status.state == BackupRunState::failed &&
                                     status.error &&
                                     status.error->message() == "offline";
                          })));
    expectCleanup();
    start();
}

TEST_F(BackupRunTest, CleanupFailureDoesNotReplaceSuccessfulBackupState) {
    InSequence sequence;
    expectWorkspace();
    expectState(BackupRunState::snapshotting);
    EXPECT_CALL(snapshotter, snapshot(_)).WillOnce(Return(metadata));
    expectState(BackupRunState::uploading);
    EXPECT_CALL(store, put(_, _)).WillOnce(Return(metadata));
    expectState(BackupRunState::completed);
    expectCleanup(true);
    start();
}

}  // namespace anchor::test
