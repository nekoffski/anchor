// #include "BackupJob.hh"

// #include "base/Functional.hh"

// namespace anchor {

// BackupJob::BackupJob(
//     const Path& workspace, const BackupPlan& backup, const FileSystem& m_fs,
//     Snapshotter& snapshotter, SnapshotStore& snapshotStore
// )
//     : m_uuid(RandomEngine::get().uuid()),
//       m_workspace(workspace),
//       m_backup(backup),
//       m_fs(m_fs),
//       m_snapshotter(snapshotter),
//       m_snapshotStore(snapshotStore),
//       m_status(std::make_shared<BackupJobStatus>()) {}

// std::shared_ptr<BackupJobStatus> BackupJob::status() const { return m_status;
// }

// void BackupJob::start() {
//     log::debug("Starting backup job: {}", m_uuid);

//     auto workspacePath = Path::join(m_workspace, m_uuid);
//     if (auto res = m_fs.createDirectory(workspacePath); not res) {
//         log::error("Failed to create workspace directory: '{}'",
//         res.error()); return;
//     }

//     auto res =
//         m_snapshotter.snapshot(m_backup.name, m_backup.filters,
//         workspacePath);

//     // DEFER { m_fs.remove(workspacePath); };
// }

// }  // namespace anchor
