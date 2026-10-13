#include "InMemoryBackupRunRepo.hh"

namespace anchor {

bool InMemoryBackupRunRepo::has(const Str& backupName) const {
    std::shared_lock lk{m_mutex};
    return m_backupRuns.contains(backupName);
}

Opt<BackupRunStatus> InMemoryBackupRunRepo::get(const Str& backupName) const {
    std::shared_lock lk{m_mutex};
    if (auto it = m_backupRuns.find(backupName); it != m_backupRuns.end()) {
        return it->second;
    }
    return std::nullopt;
}

void InMemoryBackupRunRepo::put(
    const Str& backupName, const BackupRunStatus& status
) {
    std::unique_lock lk{m_mutex};
    m_backupRuns[backupName] = status;
}

void InMemoryBackupRunRepo::remove(const Str& backupName) {
    std::unique_lock lk{m_mutex};
    m_backupRuns.erase(backupName);
}

}  // namespace anchor
