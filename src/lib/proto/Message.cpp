#include "Message.hh"

#include "BackupMessages.hh"
#include "SnapshotMessages.hh"
#include "SystemMessages.hh"
#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor::proto {

Result<std::unique_ptr<Message>> Message::create(const kc::Json& json) {
    if (not json.is_object() or not json.contains("name") or
        not json["name"].is_string() or
        json["name"].get_ref<const Str&>().empty()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Missing message name"
        );
    }

    if (not json.contains("body")) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Missing message body"
        );
    }

    const auto& name = json["name"];
    const auto& body = json["body"];

    if (name == "DiagnosticRequest") {
        return DiagnosticRequest::create(body);
    } else if (name == "AckResponse") {
        return AckResponse::create(body);
    } else if (name == "TriggerBackupRequest") {
        return TriggerBackupRequest::create(body);
    } else if (name == "ErrorResponse") {
        return ErrorResponse::create(body);
    } else if (name == "GetBackupsRequest") {
        return GetBackupsRequest::create(body);
    } else if (name == "GetBackupsResponse") {
        return GetBackupsResponse::create(body);
    } else if (name == "ReloadPlansRequest") {
        return ReloadPlansRequest::create(body);
    } else if (name == "GetSnapshotsRequest") {
        return GetSnapshotsRequest::create(body);
    } else if (name == "GetSnapshotRequest") {
        return GetSnapshotRequest::create(body);
    } else if (name == "RemoveSnapshotRequest") {
        return RemoveSnapshotRequest::create(body);
    } else if (name == "GetSnapshotsResponse") {
        return GetSnapshotsResponse::create(body);
    } else if (name == "GetSnapshotResponse") {
        return GetSnapshotResponse::create(body);
    }
    return Error::unexpected(ErrorCode::invalidRequest, "Unknown message name");
}

kc::Json Message::serialize() const {
    kc::Json json;
    json["name"] = name();
    json["body"] = serializeBody();
    return json;
}

}  // namespace anchor::proto
