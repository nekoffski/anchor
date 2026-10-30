#include "InMemoryBackupRepo.hh"

#include "core/Core.hh"

namespace anchor {

kc::Opt<Backup> InMemoryBackupRepo::get(const Str& backupName) const {
    std::shared_lock lk{m_mutex};
    if (auto it = m_backups.find(backupName); it != m_backups.end()) {
        return it->second;
    }
    return {};
}

std::vector<Backup> InMemoryBackupRepo::getAll() const {
    std::shared_lock lk{m_mutex};
    return m_backups | std::views::values |
           std::ranges::to<std::vector<Backup>>();
}

void InMemoryBackupRepo::put(const Backup& backup) {
    std::unique_lock lk{m_mutex};
    m_backups[backup.plan.name] = backup;
}

void InMemoryBackupRepo::putPlan(const BackupPlan& plan) {
    std::unique_lock lk{m_mutex};
    m_backups[plan.name].plan = plan;
}

void InMemoryBackupRepo::forEach(Callback c) const {
    std::shared_lock lk{m_mutex};
    for (const auto& [name, backup] : m_backups) {
        c(backup);
    }
}

kc::Opt<BackupRunStatus> InMemoryBackupRepo::getRunStatus(
    const Str& backupName
) const {
    std::shared_lock lk{m_mutex};
    if (auto it = m_backups.find(backupName); it != m_backups.end()) {
        return it->second.run;
    }
    return {};
}

void InMemoryBackupRepo::updateRunStatus(
    const Str& backupName, const BackupRunStatus& status
) {
    std::unique_lock lk{m_mutex};
    if (auto it = m_backups.find(backupName); it != m_backups.end()) {
        it->second.run = status;
    }
}

void InMemoryBackupRepo::clearRunStatus(const Str& backupName) {
    std::unique_lock lk{m_mutex};
    if (auto it = m_backups.find(backupName); it != m_backups.end()) {
        it->second.run.reset();
    }
}

}  // namespace anchor
