#include "BackupService.hh"

#include "base/Log.hh"

namespace anchor {

BackupService::BackupService(const Config& cfg, BackupPlanRepository& planRepo)
    : m_cfg(cfg), m_planRepo(planRepo) {
    for (const auto& plan : m_planRepo.getAll()) {
        log::info("Loaded backup plan: {}", plan.name);
        m_backups[plan.name] = Backup{plan};
    }
}

}  // namespace anchor
