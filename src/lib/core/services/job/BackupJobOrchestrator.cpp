#include "BackupJobOrchestrator.hh"

namespace anchor {

BackupJobOrchestrator::BackupJobOrchestrator(
    const FileSystem& m_fs, const Path& workspace,
    SnapshotStores& snapshotStores, Snapshotters& snapshotters
)
    : m_fs(m_fs),
      m_workspace(workspace),
      m_snapshotStores(snapshotStores),
      m_snapshotters(snapshotters) {}

Result<std::shared_ptr<BackupJobStatus>> BackupJobOrchestrator::start(
    const BackupPlan& backup
) {
    auto* snapshotter = m_snapshotters.get(backup.archiveType);

    if (not snapshotter) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Failed to create snapshotter"
        );
    }

    auto* store = m_snapshotStores.get(backup.destination.type);

    if (not store) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Failed to create snapshot store"
        );
    }

    // mock for now !!
    BackupJob job{m_workspace, backup, m_fs, *snapshotter, *store};
    auto status = job.status();
    job.start();

    return status;
}

}  // namespace anchor
