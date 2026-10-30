#include "AckResponse.hh"

#include "core/Error.hh"

namespace anchor::proto {

AckResponse::AckResponse() : MessageImpl("AckResponse") {}

Result<std::unique_ptr<AckResponse>> AckResponse::create(const kc::Json& body) {
    if (not body.is_null()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid acknowledgement body"
        );
    }
    return std::make_unique<AckResponse>();
}

kc::Json AckResponse::serializeBody() const { return nullptr; }

}  // namespace anchor::proto
