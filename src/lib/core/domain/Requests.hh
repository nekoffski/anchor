#pragma once

#include <variant>

#include "Backup.hh"
#include "base/Error.hh"

namespace anchor {

struct DiagnosticRequest {
    Str details;
};

struct TriggerBackupRequest {
    Str name;
};

struct GetBackupsRequest {};

struct GetBackupsResponse {
    std::vector<Backup> backups;
};

struct AckResponse {};

struct ErrorResponse {
    Error error;
};

using Request =
    std::variant<DiagnosticRequest, TriggerBackupRequest, GetBackupsRequest>;

using Response = std::variant<AckResponse, ErrorResponse, GetBackupsResponse>;

}  // namespace anchor
