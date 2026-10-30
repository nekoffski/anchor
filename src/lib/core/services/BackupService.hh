#pragma once

#include "BackupOrchestrator.hh"
#include "BackupScheduler.hh"
#include "core/Config.hh"
#include "core/Error.hh"
#include "core/ports/Ports.hh"

namespace anchor {

class BackupService : public kc::Thread {
   public:
    explicit BackupService(
        const Config& cfg, const kc::FileSystem& fs, Ports ports
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
    const kc::FileSystem& m_fs;
    Ports m_ports;
    RequestQueue m_requests;
    BackupOrchestrator m_orchestrator;
    BackupScheduler m_scheduler;
};

}  // namespace anchor
