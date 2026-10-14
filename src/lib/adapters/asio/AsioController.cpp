#include "AsioController.hh"

#include "proto/AckResponse.hh"
#include "proto/DiagnosticRequest.hh"
#include "proto/ErrorResponse.hh"
#include "proto/TriggerBackupRequest.hh"

namespace anchor {

AsioController::AsioController(RequestSender sender) : m_sender(sender) {}

std::unique_ptr<proto::Message> anchor::AsioController::handle(
    const proto::Message& message
) {
    if (const auto* req = message.as<proto::DiagnosticRequest>()) {
        if (not m_sender.send(DiagnosticRequest{req->details()})) {
            return std::make_unique<proto::ErrorResponse>(Error{
                ErrorCode::connectionFailure,
                "Failed to send request, server is closing"
            });
        }
        return std::make_unique<proto::AckResponse>();
    } else if (const auto* req = message.as<proto::TriggerBackupRequest>()) {
        if (not m_sender.send(TriggerBackupRequest{req->backupName()})) {
            return std::make_unique<proto::ErrorResponse>(Error{
                ErrorCode::connectionFailure,
                "Failed to send request, server is closing"
            });
        }
        return std::make_unique<proto::AckResponse>();
    }

    return std::make_unique<proto::ErrorResponse>(
        Error{ErrorCode::invalidRequest, "Invalid request"}
    );
}

}  // namespace anchor
