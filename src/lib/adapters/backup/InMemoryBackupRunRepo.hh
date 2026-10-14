#pragma once

#include <shared_mutex>
#include <unordered_map>

#include "base/Core.hh"
#include "core/ports/Backup.hh"

namespace anchor {

class InMemoryBackupRunRepo : public BackupRunRepo {
   public:
    bool has(const Str& backupName) const override;
    Opt<BackupRunStatus> get(const Str& backupName) const override;
    void put(const Str& backupName, const BackupRunStatus& status) override;
    void remove(const Str& backupName) override;
    void remove(std::span<const Str>) override;
    void forEach(Callback c) const override;

   private:
    std::unordered_map<Str, BackupRunStatus> m_backupRuns;
    mutable std::shared_mutex m_mutex;
};

}  // namespace anchor
