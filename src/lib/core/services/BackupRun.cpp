#include "BackupRun.hh"

#include "SourceTreeBuilder.hh"
#include "base/Functional.hh"
#include "base/Random.hh"

namespace anchor {

BackupRun::BackupRun(const BackupRunArgs& args)
    : m_plan(args.plan),
      m_backupRepo(args.backupRepo),
      m_snapshotter(args.snapshotter),
      m_snapshotStore(args.snapshotStore),
      m_fs(args.fs),
      m_workspace(Path::join(args.workspace, uuid())) {}

Str newSnapshotName(const Str& backupName) {
    const auto ts = std::chrono::system_clock::now();
    return fmt::format("{}_{}", backupName, ts.time_since_epoch().count());
}

void BackupRun::start() {
    if (not m_fs.createDirectory(m_workspace)) {
        fail(
            ErrorCode::fileSystemError, "Failed to create workspace directory"
        );
        return;
    }
    DEFER {
        log::debug("Cleaning up workspace: {}", m_workspace.str());
        if (auto res = m_fs.removeAll(m_workspace); not res) {
            log::error(
                "Failed to remove workspace: {} - {}", m_workspace.str(),
                res.error().message()
            );
        }
    };

    update(BackupRunState::snapshotting);

    auto trees = SourceTreeBuilder::build(m_fs, m_plan.sources);

    auto metadata = m_snapshotter.snapshot(
        SnapshotArgs{
            .name = newSnapshotName(m_plan.name),
            .trees = trees,
            .workspace = m_workspace,
        }
    );

    if (not metadata) {
        fail(metadata.error());
        return;
    }

    update(BackupRunState::uploading);

    auto postMetadata =
        m_snapshotStore.put(metadata.value(), m_plan.destination);

    if (not postMetadata) {
        fail(postMetadata.error());
        return;
    }

    update(BackupRunState::completed);
}

void BackupRun::fail(ErrorCode code, const Str& reason) {
    fail(Error{code, reason});
}

void BackupRun::fail(const Error& error) {
    log::error("Backup run failed: {}", error);

    BackupRunStatus status{
        .state = BackupRunState::failed,
        .error = error,
        .uuid = uuid(),
    };
    m_backupRepo.updateRunStatus(m_plan.name, status);
}

void BackupRun::update(BackupRunState state) {
    BackupRunStatus status{.state = state, .uuid = uuid()};
    m_backupRepo.updateRunStatus(m_plan.name, status);
}

}  // namespace anchor
