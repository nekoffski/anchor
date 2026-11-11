#include "AsioController.hh"

#include <kcore/Functional.hh>

#include "core/Error.hh"
#include "proto/BackupMessages.hh"
#include "proto/SnapshotMessages.hh"
#include "proto/SystemMessages.hh"

namespace anchor {

namespace {

Result<Request> translateRequest(const proto::Message& message) {
    if (const auto* req = message.as<proto::DiagnosticRequest>()) {
        return DiagnosticRequest{req->details()};
    } else if (const auto* req = message.as<proto::TriggerBackupRequest>()) {
        return TriggerBackupRequest{req->backupName()};
    } else if (const auto* _ = message.as<proto::GetBackupsRequest>()) {
        return GetBackupsRequest{};
    } else if (const auto* _ = message.as<proto::ReloadPlansRequest>()) {
        return ReloadPlansRequest{};
    } else if (const auto* req = message.as<proto::GetSnapshotsRequest>()) {
        return GetSnapshotsRequest{req->backupName()};
    } else if (const auto* req = message.as<proto::GetSnapshotRequest>()) {
        return GetSnapshotRequest{req->backupName(), req->uuid()};
    } else if (const auto* req = message.as<proto::RemoveSnapshotRequest>()) {
        return RemoveSnapshotRequest{req->backupName(), req->uuid()};
    }
    return Error::unexpected(
        ErrorCode::unknownRequest, "Unknown request type: {}", message.name()
    );
}

using ResponsePtr = std::unique_ptr<proto::Message>;

ResponsePtr translateResponse(const Response& request) {
    return std::visit(
        kc::Overloader{
            [](const AckResponse&) -> ResponsePtr {
                return std::make_unique<proto::AckResponse>();
            },
            [](const ErrorResponse& err) -> ResponsePtr {
                return std::make_unique<proto::ErrorResponse>(err.error);
            },
            [](const GetBackupsResponse& res) -> ResponsePtr {
                return std::make_unique<proto::GetBackupsResponse>(res.backups);
            },
            [](const GetSnapshotsResponse& response) -> ResponsePtr {
                return std::make_unique<proto::GetSnapshotsResponse>(
                    response.snapshots
                );
            },
            [](const GetSnapshotResponse&) -> ResponsePtr {
                return std::make_unique<proto::GetSnapshotResponse>();
            }
        },
        request
    );
}

}  // namespace

AsioController::AsioController(RequestSender sender) : m_sender(sender) {}

std::unique_ptr<proto::Message> anchor::AsioController::handle(
    const proto::Message& message
) {
    auto request = translateRequest(message);

    if (not request) {
        return proto::ErrorResponse::create(request.error());
    }

    auto responseFuture = m_sender.send(std::move(*request));

    if (not responseFuture) {
        return proto::ErrorResponse::create(responseFuture.error());
    }

    // FIXME: blocking for now but we should consider polling/completion token
    return translateResponse(responseFuture->get());
}

}  // namespace anchor
