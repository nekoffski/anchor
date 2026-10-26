#include "GetBackupsResponse.hh"

namespace anchor::proto {

GetBackupsResponse::GetBackupsResponse(std::span<const Backup> backups)
    : MessageImpl("GetBackupsResponse"),
      m_backups(backups.begin(), backups.end()) {}

GetBackupsResponse::GetBackupsResponse() : MessageImpl("GetBackupsResponse") {}

Result<std::unique_ptr<GetBackupsResponse>> GetBackupsResponse::create(
    const Json& json
) {
    std::vector<Backup> backups;

    for (const auto& backupJson : json) {
        Backup backup;

        backup.plan.name = backupJson["plan"]["name"].get<std::string>();
        backup.enabled = backupJson["enabled"].get<bool>();

        if (auto run = json.find("run"); run != json.end()) {
            backup.run =
                BackupRunStatus{.uuid = (*run)["uuid"].get<std::string>()};
        }
        backups.push_back(backup);
    }
    return std::make_unique<GetBackupsResponse>(backups);
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
