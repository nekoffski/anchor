#include "InMemoryBackupRepo.hh"

namespace anchor {

Opt<Backup> InMemoryBackupRepo::get(const Str& backupName) const {
    std::shared_lock lk{m_mutex};
    if (auto it = m_backups.find(backupName); it != m_backups.end()) {
        return it->second;
    }
    return {};
}

void InMemoryBackupRepo::put(const Backup& backup) {
    std::unique_lock lk{m_mutex};
    m_backups[backup.plan.name] = backup;
}

void InMemoryBackupRepo::forEach(Callback c) const {
    std::shared_lock lk{m_mutex};
    for (const auto& [name, backup] : m_backups) {
        c(backup);
    }
}

Opt<BackupRunStatus> InMemoryBackupRepo::getRunStatus(
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
