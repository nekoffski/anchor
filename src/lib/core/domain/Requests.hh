#pragma once

#include <variant>

#include "Backup.hh"
#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor {

struct DiagnosticRequest {
    Str details;
};

struct TriggerBackupRequest {
    Str name;
    bool scheduled{false};
};

struct GetBackupsRequest {};

struct ReloadPlansRequest {};

struct GetSnapshotsRequest {
    Str backupName;
};

struct GetSnapshotRequest {
    Str backupName;
    Uuid uuid;
};

struct RemoveSnapshotRequest {
    Str backupName;
    Uuid uuid;
};

struct GetSnapshotsResponse {};
struct GetSnapshotResponse {};

struct GetBackupsResponse {
    std::vector<Backup> backups;
};

struct AckResponse {};

struct ErrorResponse {
    Error error;
};

using Request = std::variant<
    DiagnosticRequest, TriggerBackupRequest, GetBackupsRequest,
    ReloadPlansRequest, GetSnapshotsRequest, GetSnapshotRequest,
    RemoveSnapshotRequest>;

using Response = std::variant<
    AckResponse, ErrorResponse, GetBackupsResponse, GetSnapshotsResponse,
    GetSnapshotResponse>;

}  // namespace anchor
