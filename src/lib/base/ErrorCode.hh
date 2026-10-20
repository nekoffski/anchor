#pragma once

#include "Core.hh"

namespace anchor {

enum class ErrorCode {
    noError,
    queueError,
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
    archiverFailure,
    notImplemented,
    alreadyRunning
};

}  // namespace anchor
