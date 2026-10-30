#pragma once

#include <kcore/Error.hh>

namespace anchor {

enum class ErrorCode {
    invalidRequest = 1,
    archiverFailure,
    unknownRequest,
    alreadyRunning,
};

const std::error_category& errorCategory() noexcept;
std::error_code make_error_code(ErrorCode code) noexcept;

using Error = kc::Error;
template <typename T>
using Result = kc::Result<T>;

}  // namespace anchor

template <>
struct std::is_error_code_enum<anchor::ErrorCode> : std::true_type {};
