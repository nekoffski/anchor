#pragma once

#include <memory>

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "base/Random.hh"
#include "core/domain/Backup.hh"
#include "core/ports/SnapshotStore.hh"
#include "core/ports/Snapshotter.hh"

namespace anchor {

enum class BackupJobState {
    starting,
    archiveTing,
    uploading,
    completed,
    failed
};

class BackupJobStatus : public NonCopyable, public NonMovable {
   public:
   private:
};

class BackupJob : public NonCopyable, public NonMovable {
   public:
    explicit BackupJob(
        const Path& workspace, const BackupPlan& backup, const FileSystem& m_fs,
        Snapshotter& snapshotter, SnapshotStore& snapshotStore
    );

    std::shared_ptr<BackupJobStatus> status() const;

    void start();

   private:
    Uuid m_uuid;
    Path m_workspace;
    BackupPlan m_backup;
    const FileSystem& m_fs;
    std::shared_ptr<BackupJobStatus> m_status;

    Snapshotter& m_snapshotter;
    SnapshotStore& m_snapshotStore;
};

}  // namespace anchor
