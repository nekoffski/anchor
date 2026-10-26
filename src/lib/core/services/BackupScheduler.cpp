#include "BackupScheduler.hh"

namespace anchor {

BackupScheduler::BackupScheduler(
    BackupPreflighter& preflighter, BackupOrchestrator& orchestrator
)
    : Thread("BackupScheduler"),
      m_preflighter(preflighter),
      m_orchestrator(orchestrator) {}

void BackupScheduler::stop() {}

void BackupScheduler::add(const Backup& backup) {}

void BackupScheduler::run() {}

}  // namespace anchor
