#include "ErrorResponse.hh"

namespace anchor::proto {

ErrorResponse::ErrorResponse(ErrorCode code, const Str& message)
    : ErrorResponse(Error{code, message}) {}

ErrorResponse::ErrorResponse(const Error& error)
    : MessageImpl("ErrorResponse"), m_error(error) {}

Result<std::unique_ptr<ErrorResponse>> ErrorResponse::create(const Json& body) {
    if (not body.is_object() or not body.contains("code") or
        not body["code"].is_number_integer() or not body.contains("message") or
        not body["message"].is_string() or body["code"] < 0 or
        body["code"] > std::to_underlying(ErrorCode::alreadyRunning)) {
        return Error::unexpected(
            ErrorCode::invalidArgument, "Invalid error response body"
        );
    }
    return std::make_unique<ErrorResponse>(
        static_cast<ErrorCode>(body["code"].get<int>()),
        body["message"].get<Str>()
    );
}

std::unique_ptr<ErrorResponse> ErrorResponse::create(const Error& error) {
    return std::make_unique<ErrorResponse>(error);
}

Json ErrorResponse::serializeBody() const {
    return Json{
        {"message", m_error.message()},
        {"code", std::to_underlying(m_error.code())}
    };
}

}  // namespace anchor::proto
