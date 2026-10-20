#pragma once

#include <shared_mutex>
#include <unordered_map>

#include "base/Core.hh"
#include "core/ports/Backup.hh"

namespace anchor {

class InMemoryBackupRepo : public BackupRepo {
   public:
    Opt<Backup> get(const Str& backupName) const override;
    void put(const Backup& backup) override;
    void forEach(Callback c) const override;

    Opt<BackupRunStatus> getRunStatus(const Str& backupName) const override;

    void updateRunStatus(
        const Str& backupName, const BackupRunStatus& status
    ) override;
    void clearRunStatus(const Str& backupName) override;

   private:
    std::unordered_map<Str, Backup> m_backups;
    mutable std::shared_mutex m_mutex;
};

}  // namespace anchor
