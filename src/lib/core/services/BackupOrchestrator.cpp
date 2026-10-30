#include "BackupOrchestrator.hh"

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

BackupOrchestrator::BackupOrchestrator(BackupRepo& repo, Executor& executor)
    : kc::Thread("BackupOrchestrator"), m_repo(repo), m_executor(executor) {}

BackupOrchestrator::~BackupOrchestrator() {
    stop();
    join();
}

Result<Uuid> BackupOrchestrator::enqueueBackup(BackupRunArgs&& args) {
    const auto name = args.plan.name;
    auto run = std::make_unique<BackupRun>(std::move(args));
    const auto uuid = run->uuid();

    BackupRunStatus initialStatus{
        .state = BackupRunState::queued, .uuid = uuid
    };

    const auto previous = m_repo.getRunStatus(name);
    m_repo.updateRunStatus(name, initialStatus);

    if (not m_queue.push(std::move(run))) {
        if (previous) {
            m_repo.updateRunStatus(name, *previous);
        } else {
            m_repo.clearRunStatus(name);
        }
        return Error::unexpected(
            kc::ErrorCode::internalError, "Failed to enqueue backup"
        );
    }

    return uuid;
}

void BackupOrchestrator::stop() { m_queue.close(); }

void BackupOrchestrator::run() {
    while (auto run = m_queue.pop()) {
        trigger(std::move(*run));
    }
}

void BackupOrchestrator::trigger(std::unique_ptr<BackupRun> run) {
    m_executor.call([run = std::move(run)]() mutable { run->start(); });
}

}  // namespace anchor
