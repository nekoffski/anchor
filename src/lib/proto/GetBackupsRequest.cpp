#include "GetBackupsRequest.hh"

namespace anchor::proto {

GetBackupsRequest::GetBackupsRequest() : MessageImpl("GetBackupsRequest") {}

Result<std::unique_ptr<GetBackupsRequest>> GetBackupsRequest::create(
    const Json&
) {
    return std::make_unique<GetBackupsRequest>();
}

Json GetBackupsRequest::serializeBody() const { return Json{}; }

}  // namespace anchor::proto
