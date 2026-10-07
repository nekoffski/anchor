#pragma once

#include "BackupPlanRepository.hh"
#include "SnapshotStore.hh"
#include "Snapshotter.hh"

namespace anchor {

class Ports {
   public:
    explicit Ports(
        BackupPlanRepository& backupPlanRepository, Snapshotters& snapshotters,
        SnapshotStores& snapshotStores
    )
        : m_backupPlanRepository(backupPlanRepository),
          m_snapshotters(snapshotters),
          m_snapshotStores(snapshotStores) {}

    BackupPlanRepository& planRepo() { return m_backupPlanRepository; }
    SnapshotStores& snapshotStores() { return m_snapshotStores; }
    Snapshotters& snapshotters() { return m_snapshotters; }

   private:
    BackupPlanRepository& m_backupPlanRepository;
    Snapshotters& m_snapshotters;
    SnapshotStores& m_snapshotStores;
};

}  // namespace anchor
