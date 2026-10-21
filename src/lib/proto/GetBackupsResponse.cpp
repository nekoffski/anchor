#include "GetBackupsResponse.hh"

namespace anchor::proto {

GetBackupsResponse::GetBackupsResponse(std::span<const Backup> backups)
    : MessageImpl("GetBackupsResponse"),
      m_backups(backups.begin(), backups.end()) {}

GetBackupsResponse::GetBackupsResponse() : MessageImpl("GetBackupsResponse") {}

Result<std::unique_ptr<GetBackupsResponse>> GetBackupsResponse::create(
    const Json&
) {
    return std::make_unique<GetBackupsResponse>();
}

Json GetBackupsResponse::serializeBody() const {
    Json body;
    for (const auto& backup : m_backups) {
        Json backupModel;

        backupModel["enabled"] = backup.enabled;
        backupModel["plan"]["name"] = backup.plan.name;

        if (backup.run) {
            backupModel["run"]["uuid"] = backup.run->uuid;
        }

        body.push_back(std::move(backupModel));
    }
    return body;
}

}  // namespace anchor::proto
