#pragma once

#include "BackupJob.hh"
#include "base/Concepts.hh"
#include "base/Core.hh"
#include "core/ports/SnapshotStore.hh"
#include "core/ports/Snapshotter.hh"

namespace anchor {

class BackupJobOrchestrator : public NonMovable, public NonCopyable {
   public:
    explicit BackupJobOrchestrator(
        const FileSystem& m_fs, const Path& workspace,
        SnapshotStores& SnapshotStores, Snapshotters& snapshotters
    );

    Result<std::shared_ptr<BackupJobStatus>> start(const BackupPlan& backup);

   private:
    const FileSystem& m_fs;
    Path m_workspace;
    SnapshotStores& m_snapshotStores;
    Snapshotters& m_snapshotters;
};

}  // namespace anchor
