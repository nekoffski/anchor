#include "AsioController.hh"

#include <kcore/Functional.hh>

#include "core/Error.hh"
#include "proto/BackupMessages.hh"
#include "proto/SnapshotMessages.hh"
#include "proto/SystemMessages.hh"

namespace anchor {

namespace {

#define TRANSLATE_REQUEST(Request, ...)                   \
    if (const auto* req = message.as<proto::Request>()) { \
        return Request{__VA_ARGS__};                      \
    }

Result<Request> translateRequest(const proto::Message& message) {
    TRANSLATE_REQUEST(DiagnosticRequest, req->details());
    TRANSLATE_REQUEST(TriggerBackupRequest, req->backupName());
    TRANSLATE_REQUEST(GetBackupsRequest);
    TRANSLATE_REQUEST(ReloadPlansRequest);
    TRANSLATE_REQUEST(GetSnapshotsRequest, req->backupName());
    TRANSLATE_REQUEST(GetSnapshotRequest, req->backupName(), req->uuid());
    TRANSLATE_REQUEST(RemoveSnapshotRequest, req->backupName(), req->uuid());

    return Error::unexpected(
        ErrorCode::unknownRequest, "Unknown request type: {}", message.name()
    );
}

using ResponsePtr = std::unique_ptr<proto::Message>;

#define TRANSLATE_RESPONSE(Response, ...)                      \
    []([[maybe_unused]] const Response& res) -> ResponsePtr {  \
        return std::make_unique<proto::Response>(__VA_ARGS__); \
    }

ResponsePtr translateResponse(const Response& request) {
    return std::visit(
        kc::Overloader{
            TRANSLATE_RESPONSE(AckResponse),
            TRANSLATE_RESPONSE(ErrorResponse, res.error),
            TRANSLATE_RESPONSE(GetBackupsResponse, res.backups),
            TRANSLATE_RESPONSE(GetSnapshotsResponse, res.snapshots),
            TRANSLATE_RESPONSE(GetSnapshotResponse)
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
