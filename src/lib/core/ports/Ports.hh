#pragma once

#include "Backup.hh"
#include "Executor.hh"
#include "Snapshot.hh"

namespace anchor {

class Ports {
   public:
    explicit Ports(
        BackupPlanRepo& BackupPlanRepo, Snapshotters& snapshotters,
        SnapshotStores& snapshotStores, Executor& executor
    )
        : m_BackupPlanRepo(BackupPlanRepo),
          m_snapshotters(snapshotters),
          m_snapshotStores(snapshotStores),
          m_executor(executor) {}

    BackupPlanRepo& planRepo() { return m_BackupPlanRepo; }
    SnapshotStores& snapshotStores() { return m_snapshotStores; }
    Snapshotters& snapshotters() { return m_snapshotters; }
    Executor& executor() { return m_executor; }

   private:
    BackupPlanRepo& m_BackupPlanRepo;
    Snapshotters& m_snapshotters;
    SnapshotStores& m_snapshotStores;
    Executor& m_executor;
};

}  // namespace anchor
