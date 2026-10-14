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

void InMemoryBackupRunRepo::remove(std::span<const Str> backupNames) {
    std::unique_lock lk{m_mutex};
    for (const auto& backupName : backupNames) {
        m_backupRuns.erase(backupName);
    }
}

void InMemoryBackupRunRepo::forEach(Callback c) const {
    std::shared_lock lk{m_mutex};
    for (const auto& [backupName, status] : m_backupRuns) {
        c(backupName, status);
    }
}

}  // namespace anchor
