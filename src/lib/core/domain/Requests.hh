#pragma once

#include <variant>

namespace anchor {

struct DiagnosticRequest {
    Str details;
};

struct TriggerBackupRequest {
    Str name;
};

using Request = std::variant<DiagnosticRequest, TriggerBackupRequest>;

}  // namespace anchor
