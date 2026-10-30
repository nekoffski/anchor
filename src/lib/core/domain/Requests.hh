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

struct GetBackupsResponse {
    std::vector<Backup> backups;
};

struct AckResponse {};

struct ErrorResponse {
    Error error;
};

using Request = std::variant<
    DiagnosticRequest, TriggerBackupRequest, GetBackupsRequest,
    ReloadPlansRequest>;

using Response = std::variant<AckResponse, ErrorResponse, GetBackupsResponse>;

}  // namespace anchor
