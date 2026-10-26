#pragma once

#include "BackupOrchestrator.hh"
#include "BackupRun.hh"
#include "BackupScheduler.hh"
#include "base/Concepts.hh"
#include "base/Config.hh"
#include "base/Core.hh"
#include "core/domain/Backup.hh"
#include "core/ports/Ports.hh"

namespace anchor {

class BackupService : public BackupPreflighter {
   public:
    explicit BackupService(
        const Config& cfg, const FileSystem& fs, Ports ports
    );

    ~BackupService();

    Result<Uuid> triggerBackup(const Str& name);
    std::vector<Backup> getBackups();

   private:
    Result<BackupRunArgs> preflightBackup(const Backup& backup) override;

    void readPlans();

    Config m_cfg;
    const FileSystem& m_fs;
    Ports m_ports;

    BackupOrchestrator m_orchestrator;
    BackupScheduler m_scheduler;
};

}  // namespace anchor
