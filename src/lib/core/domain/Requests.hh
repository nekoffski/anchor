#pragma once

#include <variant>

#include "base/Error.hh"

namespace anchor {

struct DiagnosticRequest {
    Str details;
};

struct TriggerBackupRequest {
    Str name;
};

struct AckResponse {};

struct ErrorResponse {
    Error error;
};

using Request = std::variant<DiagnosticRequest, TriggerBackupRequest>;
using Response = std::variant<AckResponse, ErrorResponse>;

}  // namespace anchor
