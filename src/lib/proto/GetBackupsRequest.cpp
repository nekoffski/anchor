#include "GetBackupsRequest.hh"

#include "core/Error.hh"

namespace anchor::proto {

GetBackupsRequest::GetBackupsRequest() : MessageImpl("GetBackupsRequest") {}

Result<std::unique_ptr<GetBackupsRequest>> GetBackupsRequest::create(
    const kc::Json& body
) {
    if (not body.is_null()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid get backups request body"
        );
    }
    return std::make_unique<GetBackupsRequest>();
}

kc::Json GetBackupsRequest::serializeBody() const { return kc::Json{}; }

}  // namespace anchor::proto
