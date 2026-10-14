#include "AckResponse.hh"

namespace anchor::proto {

AckResponse::AckResponse() : MessageImpl("AckResponse") {}

Result<std::unique_ptr<AckResponse>> AckResponse::create(const Json& body) {
    if (not body.is_null()) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Invalid acknowledgement body"
        );
    }
    return std::make_unique<AckResponse>();
}

Json AckResponse::serializeBody() const { return nullptr; }

}  // namespace anchor::proto
