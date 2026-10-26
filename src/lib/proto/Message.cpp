#include "Message.hh"

#include "AckResponse.hh"
#include "DiagnosticRequest.hh"
#include "ErrorResponse.hh"
#include "GetBackupsRequest.hh"
#include "GetBackupsResponse.hh"
#include "ReloadPlansRequest.hh"
#include "TriggerBackupRequest.hh"

namespace anchor::proto {

Result<std::unique_ptr<Message>> Message::create(const Json& json) {
    if (not json.is_object() or not json.contains("name") or
        not json["name"].is_string() or
        json["name"].get_ref<const Str&>().empty()) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Missing message name"
        );
    }

    if (not json.contains("body")) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Missing message body"
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
    }
    return Error::unexpected(ErrorCode::invalidRequest, "Unknown message name");
}

Json Message::serialize() const {
    Json json;
    json["name"] = name();
    json["body"] = serializeBody();
    return json;
}

}  // namespace anchor::proto
