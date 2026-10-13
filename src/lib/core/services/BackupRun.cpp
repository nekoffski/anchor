#include "BackupRun.hh"

#include "base/Functional.hh"
#include "base/Random.hh"

namespace anchor {

BackupRun::BackupRun(const BackupRunArgs& args)
    : m_plan(args.plan),
      m_runRepo(args.runRepo),
      m_snapshotter(args.snapshotter),
      m_snapshotStore(args.snapshotStore),
      m_fs(args.fs),
      m_workspace(Path::join(args.workspace, uuid())) {}

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

    auto metadata = m_snapshotter.snapshot(
        SnapshotArgs{
            .name = m_plan.name,
            .filters = m_plan.filters,
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
    };
    m_runRepo.put(m_plan.name, status);
}

void BackupRun::update(BackupRunState state) {
    BackupRunStatus status{.state = state};
    m_runRepo.put(m_plan.name, status);
}

}  // namespace anchor
