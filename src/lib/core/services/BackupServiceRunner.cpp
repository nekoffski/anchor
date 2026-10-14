#include "BackupServiceRunner.hh"

#include "base/Functional.hh"
#include "base/Time.hh"

namespace anchor {

void BackupServiceRunner::stop() { m_running.store(false); }

void BackupServiceRunner::run() {
    static constexpr u64 maxRequestsPerCycle = 5;

    while (m_running) {
        for (u64 i = 0; i < maxRequestsPerCycle; ++i) {
            if (auto req = m_receiver.tryReceive(); req) {
                acceptRequest(*req);
            } else {
                break;
            }
        }
        m_service.tick();
        sleepFor(100ms);
    }
}

void BackupServiceRunner::acceptRequest(Request request) {
    std::visit(
        Overloader{
            [this](const DiagnosticRequest& req) {
                log::debug("Received diagnostic request: {}", req.details);
            },
            [this](const TriggerBackupRequest& req) {
                log::debug("Received backup request: {}", req.name);
                m_service.triggerBackup(req.name);
            }
        },
        request
    );
}

BackupServiceRunner::BackupServiceRunner(
    BackupService& backupService, RequestReceiver receiver
)
    : Thread("BackupServiceRunner"),
      m_service(backupService),
      m_receiver(receiver) {}

}  // namespace anchor
