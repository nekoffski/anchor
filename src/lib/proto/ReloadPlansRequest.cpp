#include "ReloadPlansRequest.hh"

namespace anchor::proto {

ReloadPlansRequest::ReloadPlansRequest() : MessageImpl("ReloadPlansRequest") {}

Result<std::unique_ptr<ReloadPlansRequest>> ReloadPlansRequest::create(
    const Json& body
) {
    if (not body.is_null()) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Invalid reload plans request body"
        );
    }
    return std::make_unique<ReloadPlansRequest>();
}

Json ReloadPlansRequest::serializeBody() const { return nullptr; }

}  // namespace anchor::proto
