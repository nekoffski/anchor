#include "DiagnosticRequest.hh"

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor::proto {

DiagnosticRequest::DiagnosticRequest(const Str& details)
    : MessageImpl("DiagnosticRequest"), m_details(details) {}

Result<std::unique_ptr<DiagnosticRequest>> DiagnosticRequest::create(
    const kc::Json& body
) {
    if (not body.is_object() or not body.contains("details") or
        not body["details"].is_string()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument,
            "Missing or invalid diagnostic details"
        );
    }
    return std::make_unique<DiagnosticRequest>(body["details"].get<Str>());
}

kc::Json DiagnosticRequest::serializeBody() const {
    return kc::Json{{"details", m_details}};
}

}  // namespace anchor::proto
