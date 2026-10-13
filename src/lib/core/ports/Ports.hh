#pragma once

#include "Backup.hh"
#include "Executor.hh"
#include "Snapshot.hh"

namespace anchor {

class Ports {
   public:
    explicit Ports(
        BackupRunRepo& backupRunRepo, BackupPlanRepo& backupPlanRepo,
        Snapshotters& snapshotters, SnapshotStores& snapshotStores,
        Executor& executor
    )
        : m_backupRunRepo(backupRunRepo),
          m_backupPlanRepo(backupPlanRepo),
          m_snapshotters(snapshotters),
          m_snapshotStores(snapshotStores),
          m_executor(executor) {}

    BackupRunRepo& runRepo() { return m_backupRunRepo; }
    BackupPlanRepo& planRepo() { return m_backupPlanRepo; }
    SnapshotStores& snapshotStores() { return m_snapshotStores; }
    Snapshotters& snapshotters() { return m_snapshotters; }
    Executor& executor() { return m_executor; }

   private:
    BackupRunRepo& m_backupRunRepo;
    BackupPlanRepo& m_backupPlanRepo;
    Snapshotters& m_snapshotters;
    SnapshotStores& m_snapshotStores;
    Executor& m_executor;
};

}  // namespace anchor
