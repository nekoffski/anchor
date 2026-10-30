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
