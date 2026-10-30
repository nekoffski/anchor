#include "ErrorResponse.hh"

#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor::proto {

kc::Json serializeError(const Error& error) {
    return kc::Json{
        {"message", error.message()},
        {"code", error.code().value()},
        {"category", error.code().category().name()}
    };
}

ErrorResponse::ErrorResponse(std::error_code code, const Str& message)
    : ErrorResponse(Error{code, message}) {}

ErrorResponse::ErrorResponse(const Error& error)
    : MessageImpl("ErrorResponse"), m_error(error) {}

Result<std::unique_ptr<ErrorResponse>> ErrorResponse::create(
    const kc::Json& body
) {
    if (not body.is_object() or not body.contains("code") or
        not body["code"].is_number_integer() or not body.contains("message") or
        not body["message"].is_string() or not body.contains("category") or
        not body["category"].is_string() or body["code"] < 0 or
        body["code"] > std::numeric_limits<int>::max()) {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid error response body"
        );
    }
    const auto value = body["code"].get<int>();
    const auto category = body["category"].get<Str>();
    std::error_code code;
    if (category == "anchor" and value >= int(ErrorCode::invalidRequest) and
        value <= int(ErrorCode::alreadyRunning)) {
        code = static_cast<ErrorCode>(value);
    } else if (
        category == "kcore" and value <= int(kc::ErrorCode::notImplemented)
    ) {
        code = static_cast<kc::ErrorCode>(value);
    } else {
        return Error::unexpected(
            kc::ErrorCode::invalidArgument, "Invalid error category or code"
        );
    }
    return std::make_unique<ErrorResponse>(code, body["message"].get<Str>());
}

std::unique_ptr<ErrorResponse> ErrorResponse::create(const Error& error) {
    return std::make_unique<ErrorResponse>(error);
}

kc::Json ErrorResponse::serializeBody() const {
    return serializeError(m_error);
}

}  // namespace anchor::proto
