#pragma once

#include "BackupOrchestrator.hh"
#include "BackupScheduler.hh"
#include "base/Config.hh"
#include "core/ports/Ports.hh"

namespace anchor {

class BackupService : public Thread {
   public:
    explicit BackupService(
        const Config& cfg, const FileSystem& fs, Ports ports
    );
    ~BackupService() override;

    void start();
    void stop();
    RequestSender sender() { return RequestSender{m_requests}; }

   private:
    void run() override;

    Response handle(const TriggerBackupRequest& request);
    Response handle(const GetBackupsRequest& request);
    Response handle(const DiagnosticRequest& request);
    Response handle(const ReloadPlansRequest& request);

    Result<BackupRunArgs> preflightBackup(const Backup& backup);
    Result<void> readPlans();

    Config m_cfg;
    const FileSystem& m_fs;
    Ports m_ports;
    RequestQueue m_requests;
    BackupOrchestrator m_orchestrator;
    BackupScheduler m_scheduler;
};

}  // namespace anchor
