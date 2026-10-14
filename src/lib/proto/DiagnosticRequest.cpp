#include "DiagnosticRequest.hh"

namespace anchor::proto {

DiagnosticRequest::DiagnosticRequest(const Str& details)
    : MessageImpl("DiagnosticRequest"), m_details(details) {}

Result<std::unique_ptr<DiagnosticRequest>> DiagnosticRequest::create(
    const Json& body
) {
    if (not body.is_object() or not body.contains("details") or
        not body["details"].is_string()) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Missing or invalid diagnostic details"
        );
    }
    return std::make_unique<DiagnosticRequest>(body["details"].get<Str>());
}

Json DiagnosticRequest::serializeBody() const {
    return Json{{"details", m_details}};
}

}  // namespace anchor::proto
