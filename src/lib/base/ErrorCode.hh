#pragma once

#include "Core.hh"

namespace anchor {

enum class ErrorCode {
    noError,
    internalError,
    queueError,
    queueClosed,
    timeout,
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
    unknownRequest,
    alreadyRunning
};

}  // namespace anchor
