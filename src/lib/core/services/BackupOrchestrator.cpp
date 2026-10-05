#include "BackupOrchestrator.hh"

#include "base/Functional.hh"
#include "base/Time.hh"

namespace anchor {

BackupOrchestrator::BackupOrchestrator(
    const Config& cfg, BackupPlanRepository& planRepo
)
    : m_cfg(cfg), m_planRepo(planRepo) {}

void BackupOrchestrator::acceptRequest(Request request) {
    std::visit(
        Overloader{[this](const DiagnosticRequest& req) {
            log::debug("Received diagnostic request: {}", req.details);
        }},
        request
    );
}

void BackupOrchestratorRunner::stop() { m_running.store(false); }

void BackupOrchestratorRunner::run() {
    static constexpr u64 maxRequestsPerCycle = 5;

    while (m_running) {
        for (u64 i = 0; i < maxRequestsPerCycle; ++i) {
            if (auto req = m_receiver.tryReceive(); req) {
                m_orchestrator.acceptRequest(*req);
            } else {
                break;
            }
        }

        sleepFor(100ms);
    }
}

BackupOrchestratorRunner::BackupOrchestratorRunner(
    BackupOrchestrator& orchestrator, RequestReceiver receiver
)
    : Thread("BackupOrchestratorRunner"),
      m_orchestrator(orchestrator),
      m_receiver(receiver) {}

}  // namespace anchor
