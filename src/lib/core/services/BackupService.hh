#pragma once

#include "BackupRun.hh"
#include "base/Concepts.hh"
#include "base/Config.hh"
#include "base/Core.hh"
#include "core/domain/Backup.hh"
#include "core/ports/Ports.hh"

namespace anchor {

class BackupService : public NonCopyable, public NonMovable {
   public:
    explicit BackupService(
        const Config& cfg, const FileSystem& fs, Ports ports
    );

    Result<Uuid> triggerBackup(const Str& name);
    void tick();
    std::vector<Backup> getBackups();

   private:
    Result<BackupRunArgs> preflightBackup(const Backup& backup);

    void readPlans();
    void scanBackupRuns();

    Config m_cfg;
    const FileSystem& m_fs;
    Ports m_ports;
};

}  // namespace anchor
