#include "ReloadPlansRequest.hh"

#include "core/Error.hh"

namespace anchor::proto {

ReloadPlansRequest::ReloadPlansRequest() : MessageImpl("ReloadPlansRequest") {}

Result<std::unique_ptr<ReloadPlansRequest>> ReloadPlansRequest::create(
    const kc::Json& body
) {
    if (not body.is_null()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid reload plans request body"
        );
    }
    return std::make_unique<ReloadPlansRequest>();
}

kc::Json ReloadPlansRequest::serializeBody() const { return nullptr; }

}  // namespace anchor::proto
