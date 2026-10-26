#pragma once

#include "base/Concepts.hh"
#include "base/Random.hh"
#include "core/domain/Backup.hh"
#include "core/ports/Backup.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

struct BackupRunArgs {
    const BackupPlan& plan;
    BackupRepo& backupRepo;
    Snapshotter& snapshotter;
    SnapshotStore& snapshotStore;
    const FileSystem& fs;
    const Path& workspace;
};

class BackupPreflighter : public virtual NonCopyable,
                          public virtual NonMovable {
   public:
    virtual ~BackupPreflighter() = default;

    virtual Result<BackupRunArgs> preflightBackup(const Backup& backup) = 0;
};

class BackupRun : public WithUuid {
   public:
    explicit BackupRun(const BackupRunArgs& args);

    void start();

   private:
    void fail(ErrorCode code, const Str& reason);
    void fail(const Error& error);

    void update(BackupRunState state);

    BackupPlan m_plan;
    BackupRepo& m_backupRepo;
    Snapshotter& m_snapshotter;
    SnapshotStore& m_snapshotStore;
    const FileSystem& m_fs;
    Path m_workspace;
};

}  // namespace anchor
