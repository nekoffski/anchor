#include "Error.hh"

namespace anchor {
namespace {

class ErrorCategory final : public std::error_category {
   public:
    const char* name() const noexcept override { return "anchor"; }
    std::string message(int value) const override {
        switch (static_cast<ErrorCode>(value)) {
            case ErrorCode::invalidRequest:
                return "Invalid request";
            case ErrorCode::archiverFailure:
                return "Archiver failure";
            case ErrorCode::unknownRequest:
                return "Unknown request";
            case ErrorCode::alreadyRunning:
                return "Already running";
            case ErrorCode::internalError:
                return "Internal error";
            case ErrorCode::queueError:
                return "Queue error";
            case ErrorCode::eof:
                return "End of file";
            case ErrorCode::connectionFailure:
                return "Connection failure";
            case ErrorCode::transientNetworkError:
                return "Transient network error";
            case ErrorCode::bufferOverflow:
                return "Buffer overflow";
            case ErrorCode::serializationFailure:
                return "Serialization failure";
            case ErrorCode::notImplemented:
                return "Not implemented";
        }
        return "Unknown Anchor error";
    }
};

}  // namespace

const std::error_category& errorCategory() noexcept {
    static ErrorCategory category;
    return category;
}

std::error_code make_error_code(ErrorCode code) noexcept {
    return {static_cast<int>(code), errorCategory()};
}

}  // namespace anchor
