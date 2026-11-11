#include "BackupService.hh"

#include <kcore/Pipeline.hh>

#include "BackupRun.hh"
#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {
namespace {

Response toResponse(Result<Response> result) {
    if (not result) {
        return ErrorResponse{result.error()};
    }
    return std::move(*result);
}

}  // namespace

BackupService::BackupService(
    const Config& cfg, const kc::FileSystem& fs, Ports ports
)
    : kc::Thread("BackupService"),
      m_cfg(cfg),
      m_fs(fs),
      m_ports(ports),
      m_orchestrator(m_ports.backupRepo(), m_ports.executor()),
      m_scheduler(m_ports.backupRepo(), sender()) {
    if (auto result = readPlans(); not result) {
        log::error("Could not load all backup plans: {}", result.error());
    }
}

void BackupService::start() {
    m_orchestrator.start();
    kc::Thread::start();
    m_scheduler.start();
}

BackupService::~BackupService() {
    stop();
    join();
}

void BackupService::stop() {
    m_scheduler.stop();
    m_scheduler.join();
    m_requests.close();
}

void BackupService::run() {
    while (auto request = m_requests.pop()) {
        request->promise.set_value(
            std::visit(
                [&](const auto& value) { return handle(value); },
                request->request
            )
        );
    }
    m_orchestrator.stop();
    m_orchestrator.join();
}

Response BackupService::handle(const TriggerBackupRequest& request) {
    const auto& name = request.name;
    log::debug("Triggering backup: '{}'", name);

    const auto backup = m_ports.backupRepo().get(name);
    if (not backup) {
        log::error("Backup plan not found: '{}'", name);
        return ErrorResponse{
            Error{kc::ErrorCode::invalidArgument, "Backup plan not found"}
        };
    }

    if (request.scheduled && (not backup->enabled ||
                              (backup->run && isActive(backup->run->state)))) {
        log::debug("Skipping scheduled backup '{}'", name);
        return AckResponse{};
    }

    auto args = preflightBackup(*backup);

    if (not args) {
        log::error("Failed to preflight backup: '{}'", name);
        return ErrorResponse{args.error()};
    }

    auto result = m_orchestrator.enqueueBackup(std::move(*args));
    if (not result) {
        log::error("Could not enqueue backup '{}': {}", name, result.error());
        return ErrorResponse{result.error()};
    }
    return AckResponse{};
}

Response BackupService::handle(const GetBackupsRequest&) {
    return GetBackupsResponse{m_ports.backupRepo().getAll()};
}

Response BackupService::handle(const DiagnosticRequest& request) {
    log::debug("Received diagnostic request: {}", request.details);
    return AckResponse{};
}

Response BackupService::handle(const ReloadPlansRequest&) {
    if (auto result = readPlans(); not result) {
        return ErrorResponse{result.error()};
    }
    return AckResponse{};
}

Response BackupService::handle(const GetSnapshotsRequest& request) {
    const auto resolve = [&] {
        return resolveSnapshotTarget(request.backupName);
    };
    const auto list = [&](const SnapshotTarget& target) {
        return target.store.list(target.destination, request.backupName);
    };
    const auto respond =
        [](std::vector<SnapshotSummary> snapshots) -> Response {
        return GetSnapshotsResponse{std::move(snapshots)};
    };
    return toResponse(
        kc::startPipeline(resolve).then(list).transform(respond).result()
    );
}

Response BackupService::handle(const GetSnapshotRequest&) {
    return GetSnapshotResponse{};
}

Response BackupService::handle(const RemoveSnapshotRequest& request) {
    const auto resolve = [&] {
        return resolveSnapshotTarget(request.backupName);
    };
    const auto remove = [&](const SnapshotTarget& target) {
        return target.store.remove(
            target.destination, request.backupName, request.uuid
        );
    };
    const auto respond = []() -> Response { return AckResponse{}; };
    return toResponse(
        kc::startPipeline(resolve).then(remove).transform(respond).result()
    );
}

Result<BackupService::SnapshotTarget> BackupService::resolveSnapshotTarget(
    const Str& backupName
) {
    const auto backup = m_ports.backupRepo().get(backupName);
    if (not backup) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Backup plan not found"
        );
    }
    auto* store = m_ports.snapshotStores().get(backup->plan.destination.type);
    if (not store) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Snapshot store not found"
        );
    }
    return SnapshotTarget{*store, backup->plan.destination};
}

Result<BackupRunArgs> BackupService::preflightBackup(const Backup& backup) {
    if (backup.run && isActive(backup.run->state)) {
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
            kc::ErrorCode::invalidArgument, "Snapshotter not found"
        );
    }

    auto* store = m_ports.snapshotStores().get(backup.plan.destination.type);

    if (not store) {
        log::error(
            "Snapshot store not found: '{}'",
            locationTypeToStr(backup.plan.destination.type)
        );
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Snapshot store not found"
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

Result<void> BackupService::readPlans() {
    return m_ports.planRepo().forEach([&](const auto& plan) {
        log::info("Loaded backup plan: '{}'", plan.name);
        for (const auto& source : plan.sources) {
            log::info("\tSource: '{}'", source.root.str());
            for (const auto& include : source.includes) {
                log::info("\t\tInclude: '{}'", include);
            }
        }

        m_ports.backupRepo().putPlan(plan);
        m_scheduler.add(plan.name);
    });
}

}  // namespace anchor
