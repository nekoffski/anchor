#include "BackupOrchestrator.hh"

#include "base/Time.hh"

namespace anchor {

BackupOrchestrator::BackupOrchestrator(BackupRepo& repo, Executor& executor)
    : Thread("BackupOrchestrator"), m_repo(repo), m_executor(executor) {}

Result<Uuid> BackupOrchestrator::enqueueBackup(BackupRunArgs&& args) {
    const auto name = args.plan.name;
    auto run = std::make_unique<BackupRun>(std::move(args));
    const auto uuid = run->uuid();

    BackupRunStatus initialStatus{
        .state = BackupRunState::queued, .uuid = uuid
    };

    m_repo.updateRunStatus(name, initialStatus);

    if (not m_queue.push(std::move(run))) {
        return Error::unexpected(
            ErrorCode::internalError, "Failed to enqueue backup"
        );
    }

    return uuid;
}

void BackupOrchestrator::stop() { m_queue.close(); }

void BackupOrchestrator::checkFinishedRuns() {
    std::vector<Str> completedBackups;

    m_repo.forEach([&](const Backup& backup) {
        if (backup.run and backup.run->state == BackupRunState::completed) {
            log::debug("Backup '{}' has completed.", backup.plan.name);
            completedBackups.push_back(backup.plan.name);
        }
    });
    for (const auto& name : completedBackups) {
        m_repo.clearRunStatus(name);
    }
}

void BackupOrchestrator::run() {
    const auto timeout = 150ms;

    for (;;) {
        auto run = m_queue.popFor(timeout);

        if (not run) {
            if (run.error().code() == ErrorCode::timeout) {
                checkFinishedRuns();
                continue;
            }
            break;
        }
        trigger(std::move(*run));
    }
}

void BackupOrchestrator::trigger(std::unique_ptr<BackupRun> run) {
    m_executor.call([run = std::move(run)]() mutable { run->start(); });
}

}  // namespace anchor
