#include "BackupService.hh"

#include "base/Log.hh"

namespace anchor {

BackupService::BackupService(
    const Config& cfg, const FileSystem& fs, Ports& ports
)
    : m_cfg(cfg),
      m_ports(ports),
      m_executor(
          fs, cfg.paths.workspace, m_ports.snapshotStores(),
          m_ports.snapshotters()
      ) {
    for (const auto& plan : m_ports.planRepo().getAll()) {
        log::info("Loaded backup plan: '{}'", plan.name);
        for (const auto& filter : plan.filters) {
            log::info("\tFilter: '{}'", filter.str());
        }
        m_backups[plan.name] = Backup{plan};
    }
}

void BackupService::triggerBackup(const Str& name) {
    if (not m_backups.contains(name)) {
        log::error("Backup plan not found: '{}'", name);
        return;
    }

    const auto& backup = m_backups[name];

    if (auto status = m_executor.start(backup.plan); status) {
        m_activeJobs[name] = std::move(status.value());
    } else {
        log::error("Failed to start backup: '{}'", name);
    }
}

}  // namespace anchor
