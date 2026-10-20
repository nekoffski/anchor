#pragma once

#include "Backup.hh"
#include "Executor.hh"
#include "Snapshot.hh"

namespace anchor {

class Ports {
   public:
    explicit Ports(
        BackupRepo& backupRepo, BackupPlanRepo& backupPlanRepo,
        Snapshotters& snapshotters, SnapshotStores& snapshotStores,
        Executor& executor
    )
        : m_backupRepo(backupRepo),
          m_backupPlanRepo(backupPlanRepo),
          m_snapshotters(snapshotters),
          m_snapshotStores(snapshotStores),
          m_executor(executor) {}

    BackupRepo& backupRepo() { return m_backupRepo; }
    BackupPlanRepo& planRepo() { return m_backupPlanRepo; }
    SnapshotStores& snapshotStores() { return m_snapshotStores; }
    Snapshotters& snapshotters() { return m_snapshotters; }
    Executor& executor() { return m_executor; }

   private:
    BackupRepo& m_backupRepo;
    BackupPlanRepo& m_backupPlanRepo;
    Snapshotters& m_snapshotters;
    SnapshotStores& m_snapshotStores;
    Executor& m_executor;
};

}  // namespace anchor
