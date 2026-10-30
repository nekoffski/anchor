#include "TriggerBackupRequest.hh"

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor::proto {

TriggerBackupRequest::TriggerBackupRequest(const Str& name)
    : MessageImpl("TriggerBackupRequest"), m_name(name) {}

Result<std::unique_ptr<TriggerBackupRequest>> TriggerBackupRequest::create(
    const kc::Json& body
) {
    if (not body.is_object() or not body.contains("name") or
        not body["name"].is_string() or
        body["name"].get_ref<const Str&>().empty()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Missing or invalid backup name"
        );
    }
    return std::make_unique<TriggerBackupRequest>(body["name"].get<Str>());
}

kc::Json TriggerBackupRequest::serializeBody() const {
    return kc::Json{{"name", m_name}};
}

}  // namespace anchor::proto
