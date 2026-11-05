#include "BackupMessages.hh"

#include "SystemMessages.hh"

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

GetBackupsRequest::GetBackupsRequest() : MessageImpl("GetBackupsRequest") {}

Result<std::unique_ptr<GetBackupsRequest>> GetBackupsRequest::create(
    const kc::Json& body
) {
    if (not body.is_null()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid get backups request body"
        );
    }
    return std::make_unique<GetBackupsRequest>();
}

kc::Json GetBackupsRequest::serializeBody() const { return kc::Json{}; }

namespace {

Result<BackupRunStatus> parseRunStatus(const kc::Json& json) {
    auto state = backupRunStateFromString(json.at("state").get<Str>());
    if (not state) {
        return Error::unexpected(state.error());
    }
    BackupRunStatus status{.state = *state, .uuid = json.at("uuid").get<Str>()};
    if (status.uuid.empty()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Run ID must not be empty"
        );
    }
    if (const auto error = json.find("error");
        error != json.end() && not error->is_null()) {
        auto parsed = ErrorResponse::create(*error);
        if (not parsed) {
            return Error::unexpected(parsed.error());
        }
        status.error = (*parsed)->error();
    }
    return status;
}

}  // namespace

GetBackupsResponse::GetBackupsResponse(std::span<const Backup> backups)
    : MessageImpl("GetBackupsResponse"),
      m_backups(backups.begin(), backups.end()) {}

GetBackupsResponse::GetBackupsResponse() : MessageImpl("GetBackupsResponse") {}

Result<std::unique_ptr<GetBackupsResponse>> GetBackupsResponse::create(
    const kc::Json& json
) {
    if (not json.is_array()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Expected an array of backups"
        );
    }
    try {
        std::vector<Backup> backups;
        for (const auto& backupJson : json) {
            Backup backup{};
            backup.plan.name = backupJson.at("plan").at("name").get<Str>();
            backup.enabled = backupJson.at("enabled").get<bool>();
            if (backup.plan.name.empty()) {
                return Error::unexpected(
                    kc::ErrorCode::invalidArgument,
                    "Backup name must not be empty"
                );
            }
            if (const auto run = backupJson.find("run");
                run != backupJson.end() && not run->is_null()) {
                auto status = parseRunStatus(*run);
                if (not status) {
                    return Error::unexpected(status.error());
                }
                backup.run = std::move(*status);
            }
            backups.push_back(std::move(backup));
        }
        return std::make_unique<GetBackupsResponse>(backups);
    } catch (const kc::Json::exception& error) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid backups response: {}",
            error.what()
        );
    }
}

kc::Json GetBackupsResponse::serializeBody() const {
    auto body = kc::Json::array();
    for (const auto& backup : m_backups) {
        kc::Json backupModel;

        backupModel["enabled"] = backup.enabled;
        backupModel["plan"]["name"] = backup.plan.name;

        if (backup.run) {
            backupModel["run"]["uuid"] = backup.run->uuid;
            backupModel["run"]["state"] =
                backupRunStateToString(backup.run->state);
            if (backup.run->error) {
                backupModel["run"]["error"] =
                    serializeError(*backup.run->error);
            }
        }

        body.push_back(std::move(backupModel));
    }
    return body;
}

ReloadPlansRequest::ReloadPlansRequest() : MessageImpl("ReloadPlansRequest") {}

Result<std::unique_ptr<ReloadPlansRequest>> ReloadPlansRequest::create(
    const kc::Json& body
) {
    if (not body.is_null()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid reload plans request body"
        );
    }
    return std::make_unique<ReloadPlansRequest>();
}

kc::Json ReloadPlansRequest::serializeBody() const { return nullptr; }

}  // namespace anchor::proto
