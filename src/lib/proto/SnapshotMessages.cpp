#include "SnapshotMessages.hh"

#include <kcore/Random.hh>

namespace anchor::proto {
namespace {

Result<void> validateBackupName(const kc::Json& body) {
    if (not body.is_object() || not body.contains("name") ||
        not body["name"].is_string() ||
        body["name"].get_ref<const Str&>().empty()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Missing or invalid backup name"
        );
    }
    return {};
}

Result<void> validateSnapshot(const kc::Json& body) {
    if (auto result = validateBackupName(body); not result) {
        return result;
    }
    if (not body.contains("uuid") || not body["uuid"].is_string() ||
        not kc::isValidUuid(body["uuid"].get_ref<const Str&>())) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Missing or invalid snapshot UUID"
        );
    }
    return {};
}

}  // namespace

GetSnapshotsRequest::GetSnapshotsRequest(const Str& backupName)
    : MessageImpl("GetSnapshotsRequest"), m_backupName(backupName) {}

Result<std::unique_ptr<GetSnapshotsRequest>> GetSnapshotsRequest::create(
    const kc::Json& body
) {
    if (auto result = validateBackupName(body); not result) {
        return Error::unexpected(result.error());
    }
    return std::make_unique<GetSnapshotsRequest>(body["name"].get<Str>());
}

kc::Json GetSnapshotsRequest::serializeBody() const {
    return {{"name", m_backupName}};
}

GetSnapshotRequest::GetSnapshotRequest(const Str& backupName, const Uuid& uuid)
    : MessageImpl("GetSnapshotRequest"),
      m_backupName(backupName),
      m_uuid(uuid) {}

Result<std::unique_ptr<GetSnapshotRequest>> GetSnapshotRequest::create(
    const kc::Json& body
) {
    if (auto result = validateSnapshot(body); not result) {
        return Error::unexpected(result.error());
    }
    return std::make_unique<GetSnapshotRequest>(
        body["name"].get<Str>(), body["uuid"].get<Uuid>()
    );
}

kc::Json GetSnapshotRequest::serializeBody() const {
    return {{"name", m_backupName}, {"uuid", m_uuid}};
}

RemoveSnapshotRequest::RemoveSnapshotRequest(
    const Str& backupName, const Uuid& uuid
)
    : MessageImpl("RemoveSnapshotRequest"),
      m_backupName(backupName),
      m_uuid(uuid) {}

Result<std::unique_ptr<RemoveSnapshotRequest>> RemoveSnapshotRequest::create(
    const kc::Json& body
) {
    if (auto result = validateSnapshot(body); not result) {
        return Error::unexpected(result.error());
    }
    return std::make_unique<RemoveSnapshotRequest>(
        body["name"].get<Str>(), body["uuid"].get<Uuid>()
    );
}

kc::Json RemoveSnapshotRequest::serializeBody() const {
    return {{"name", m_backupName}, {"uuid", m_uuid}};
}

GetSnapshotsResponse::GetSnapshotsResponse()
    : MessageImpl("GetSnapshotsResponse") {}

Result<std::unique_ptr<GetSnapshotsResponse>> GetSnapshotsResponse::create(
    const kc::Json& body
) {
    if (not body.is_array() || not body.empty()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Expected an empty snapshot list"
        );
    }
    return std::make_unique<GetSnapshotsResponse>();
}

kc::Json GetSnapshotsResponse::serializeBody() const {
    return kc::Json::array();
}

GetSnapshotResponse::GetSnapshotResponse()
    : MessageImpl("GetSnapshotResponse") {}

Result<std::unique_ptr<GetSnapshotResponse>> GetSnapshotResponse::create(
    const kc::Json& body
) {
    if (not body.is_null()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument,
            "Expected an empty snapshot inspection response"
        );
    }
    return std::make_unique<GetSnapshotResponse>();
}

kc::Json GetSnapshotResponse::serializeBody() const { return nullptr; }

}  // namespace anchor::proto
