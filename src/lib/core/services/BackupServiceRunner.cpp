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
                acceptRequest(std::move(*req));
            } else {
                break;
            }
        }
        m_service.tick();
        sleepFor(100ms);
    }
}

void BackupServiceRunner::acceptRequest(RequestWrapper&& wrapper) {
    const auto res = std::visit(m_requestHandler, wrapper.request);
    wrapper.promise.set_value(res);
}

BackupServiceRunner::BackupServiceRunner(
    BackupService& backupService, RequestReceiver receiver
)
    : Thread("BackupServiceRunner"),
      m_service(backupService),
      m_receiver(receiver),
      m_requestHandler(backupService) {}

BackupServiceRunner::RequestHandler::RequestHandler(BackupService& service)
    : m_service(service) {}

Response BackupServiceRunner::RequestHandler::operator()(
    const TriggerBackupRequest& request
) {
    log::debug("Received backup request: {}", request.name);

    auto result = m_service.triggerBackup(request.name);

    if (not result) {
        return ErrorResponse{.error = result.error()};
    }

    return AckResponse{};
}

Response BackupServiceRunner::RequestHandler::operator()(
    const DiagnosticRequest& request
) {
    log::debug("Received diagnostic request: {}", request.details);
    return AckResponse{};
}

}  // namespace anchor
