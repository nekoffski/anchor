#include "BackupService.hh"

#include "BackupRun.hh"
#include "base/Log.hh"

namespace anchor {

BackupService::BackupService(
    const Config& cfg, const FileSystem& fs, Ports ports
)
    : m_cfg(cfg), m_fs(fs), m_ports(ports) {
    readPlans();
}

void BackupService::triggerBackup(const Str& name) {
    if (not m_backups.contains(name)) {
        log::error("Backup plan not found: '{}'", name);
        return;
    }

    const auto& backup = m_backups[name];

    if (m_ports.runRepo().has(name)) {
        log::info("Backup '{}' is already running.", name);
        return;
    }

    auto* snapshotter = m_ports.snapshotters().get(backup.plan.archiveType);

    if (not snapshotter) {
        log::error(
            "Snapshotter not found: '{}'",
            archiveTypeToStr(backup.plan.archiveType)
        );
        return;
    }

    auto* store = m_ports.snapshotStores().get(backup.plan.destination.type);

    if (not store) {
        log::error(
            "Snapshot store not found: '{}'",
            locationTypeToStr(backup.plan.destination.type)
        );
        return;
    }

    BackupRunStatus status{.state = BackupRunState::starting};
    m_ports.runRepo().put(name, status);

    BackupRunArgs args{
        .plan = backup.plan,
        .runRepo = m_ports.runRepo(),
        .snapshotter = *snapshotter,
        .snapshotStore = *store,
        .fs = m_fs,
        .workspace = m_cfg.paths.workspace,
    };

    m_ports.executor().call([args = std::move(args)]() {
        BackupRun{args}.start();
    });
}

void BackupService::readPlans() {
    for (const auto& plan : m_ports.planRepo().getAll()) {
        log::info("Loaded backup plan: '{}'", plan.name);
        for (const auto& filter : plan.filters) {
            log::info("\tFilter: '{}'", filter.str());
        }
        m_backups[plan.name] = Backup{plan};
    }
}

}  // namespace anchor
