#include "BackupOrchestrator.hh"

namespace anchor {

BackupOrchestrator::BackupOrchestrator(const Config& cfg)
    : Thread("BackupOrchestrator"), m_cfg(cfg) {}

void BackupOrchestrator::stop() {}

void BackupOrchestrator::run() {}

}  // namespace anchor
