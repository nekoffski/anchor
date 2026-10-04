#pragma once

#include "Core.hh"

namespace anchor {

enum class ErrorCode {
    noError,
    fileSystemError,
    invalidArgument,
    jsonParsingError,
    fileNotFound,
    eof,
    connectionFailure,
    transientNetworkError,
    bufferOverflow,
    serializationFailure
};

}  // namespace anchor
