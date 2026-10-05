#pragma once

#include <variant>

namespace anchor {

struct DiagnosticRequest {
    Str details;
};

using Request = std::variant<DiagnosticRequest>;

}  // namespace anchor
