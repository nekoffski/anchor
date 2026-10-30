#pragma once

#include <shared_mutex>
#include <unordered_map>

#include "core/Core.hh"
#include "core/ports/Backup.hh"

namespace anchor {

class InMemoryBackupRepo : public BackupRepo {
   public:
    kc::Opt<Backup> get(const Str& backupName) const override;
    std::vector<Backup> getAll() const override;

    void put(const Backup& backup) override;
    void putPlan(const BackupPlan& plan) override;
    void forEach(Callback c) const override;

    kc::Opt<BackupRunStatus> getRunStatus(const Str& backupName) const override;

    void updateRunStatus(
        const Str& backupName, const BackupRunStatus& status
    ) override;
    void clearRunStatus(const Str& backupName) override;

   private:
    std::unordered_map<Str, Backup> m_backups;
    mutable std::shared_mutex m_mutex;
};

}  // namespace anchor
