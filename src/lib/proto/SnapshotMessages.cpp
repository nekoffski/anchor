#include "SnapshotMessages.hh"

#include <kcore/Random.hh>

#include "adapters/serialization/SnapshotJson.hh"

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
      m_uuid(kc::parseUuid(uuid).value_or(uuid)) {}

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
      m_uuid(kc::parseUuid(uuid).value_or(uuid)) {}

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

GetSnapshotsResponse::GetSnapshotsResponse(
    std::span<const SnapshotSummary> snapshots
)
    : MessageImpl("GetSnapshotsResponse"),
      m_snapshots(snapshots.begin(), snapshots.end()) {}

Result<std::unique_ptr<GetSnapshotsResponse>> GetSnapshotsResponse::create(
    const kc::Json& body
) {
    if (not body.is_array()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Expected an array of snapshots"
        );
    }
    std::vector<SnapshotSummary> snapshots;
    for (const auto& value : body) {
        auto snapshot = parseSnapshotSummary(value);
        if (not snapshot) {
            return Error::unexpected(snapshot.error());
        }
        snapshots.push_back(std::move(*snapshot));
    }
    return std::make_unique<GetSnapshotsResponse>(snapshots);
}

kc::Json GetSnapshotsResponse::serializeBody() const {
    auto body = kc::Json::array();
    for (const auto& snapshot : m_snapshots) {
        body.push_back(serializeSnapshotSummary(snapshot));
    }
    return body;
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
