#pragma once

#include "Core.hh"

namespace anchor {

enum class ErrorCode {
    noError,
    invalidConfig,
    fileSystemError,
    invalidArgument,
    jsonParsingError,
    fileNotFound,
    invalidRequest,
    eof,
    connectionFailure,
    transientNetworkError,
    bufferOverflow,
    serializationFailure,
};

}  // namespace anchor
