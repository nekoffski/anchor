#include "BackupService.hh"

#include "BackupRun.hh"
#include "base/Log.hh"

namespace anchor {

BackupService::BackupService(
    const Config& cfg, const FileSystem& fs, Ports ports
)
    : m_cfg(cfg),
      m_fs(fs),
      m_ports(ports),
      m_orchestrator(m_ports.backupRepo(), m_ports.executor()),
      m_scheduler(*this, m_orchestrator) {
    readPlans();

    m_orchestrator.start();
    m_scheduler.start();
}

BackupService::~BackupService() {
    m_orchestrator.stop();
    m_scheduler.stop();
}

Result<Uuid> BackupService::triggerBackup(const Str& name) {
    log::debug("Triggering backup: '{}'", name);

    const auto backup = m_ports.backupRepo().get(name);
    if (not backup) {
        log::error("Backup plan not found: '{}'", name);
        return Error::unexpected(
            ErrorCode::invalidArgument, "Backup plan not found"
        );
    }

    auto args = preflightBackup(*backup);

    if (not args) {
        log::error("Failed to preflight backup: '{}'", name);
        return Error::unexpected(args.error());
    }

    return m_orchestrator.enqueueBackup(std::move(*args));
}

std::vector<Backup> BackupService::getBackups() {
    return m_ports.backupRepo().getAll();
}

Result<BackupRunArgs> BackupService::preflightBackup(const Backup& backup) {
    if (backup.run) {
        log::info("Backup '{}' is already running.", backup.plan.name);
        return Error::unexpected(
            ErrorCode::alreadyRunning, "Backup is already running"
        );
    }

    auto* snapshotter = m_ports.snapshotters().get(backup.plan.archiveType);

    if (not snapshotter) {
        log::error(
            "Snapshotter not found: '{}'",
            archiveTypeToStr(backup.plan.archiveType)
        );
        return Error::unexpected(
            ErrorCode::invalidArgument, "Snapshotter not found"
        );
    }

    auto* store = m_ports.snapshotStores().get(backup.plan.destination.type);

    if (not store) {
        log::error(
            "Snapshot store not found: '{}'",
            locationTypeToStr(backup.plan.destination.type)
        );
        return Error::unexpected(
            ErrorCode::invalidArgument, "Snapshot store not found"
        );
    }

    return BackupRunArgs{
        .plan = backup.plan,
        .backupRepo = m_ports.backupRepo(),
        .snapshotter = *snapshotter,
        .snapshotStore = *store,
        .fs = m_fs,
        .workspace = m_cfg.paths.workspace,
    };
}

void BackupService::readPlans() {
    m_ports.planRepo().forEach([&](const auto& plan) {
        log::info("Loaded backup plan: '{}'", plan.name);
        for (const auto& source : plan.sources) {
            log::info("\tSource: '{}'", source.root.str());
            for (const auto& include : source.includes) {
                log::info("\t\tInclude: '{}'", include);
            }
        }

        auto backup =
            m_ports.backupRepo().get(plan.name).value_or(Backup{plan});
        backup.plan = plan;
        m_ports.backupRepo().put(backup);
    });
}

}  // namespace anchor
