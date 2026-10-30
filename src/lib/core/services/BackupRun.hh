#pragma once

#include <kcore/Concepts.hh>
#include <kcore/Random.hh>

#include "core/Core.hh"
#include "core/Error.hh"
#include "core/domain/Backup.hh"
#include "core/ports/Backup.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

struct BackupRunArgs {
    const BackupPlan& plan;
    BackupRepo& backupRepo;
    Snapshotter& snapshotter;
    SnapshotStore& snapshotStore;
    const kc::FileSystem& fs;
    const kc::Path& workspace;
};

class BackupRun : public kc::WithUuid {
   public:
    explicit BackupRun(const BackupRunArgs& args);

    void start();

   private:
    void fail(std::error_code code, const Str& reason);
    void fail(const Error& error);

    void update(BackupRunState state);

    BackupPlan m_plan;
    BackupRepo& m_backupRepo;
    Snapshotter& m_snapshotter;
    SnapshotStore& m_snapshotStore;
    const kc::FileSystem& m_fs;
    kc::Path m_workspace;
};

}  // namespace anchor
