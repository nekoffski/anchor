#pragma once

#include <kcore/Error.hh>

namespace anchor {

enum class ErrorCode {
    invalidRequest = 1,
    archiverFailure = 2,
    unknownRequest = 3,
    alreadyRunning = 4,
    internalError = 5,
    queueError = 6,
    eof = 7,
    connectionFailure = 8,
    transientNetworkError = 9,
    bufferOverflow = 10,
    serializationFailure = 11,
    notImplemented = 12,
};

const std::error_category& errorCategory() noexcept;
std::error_code make_error_code(ErrorCode code) noexcept;

using Error = kc::Error;

template <typename T>
using Result = kc::Result<T>;

}  // namespace anchor

template <>
struct std::is_error_code_enum<anchor::ErrorCode> : std::true_type {};
