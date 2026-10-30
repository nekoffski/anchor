#pragma once

#include "Message.hh"
#include "core/Core.hh"
#include "core/Error.hh"

namespace anchor::proto {

kc::Json serializeError(const Error& error);

class ErrorResponse : public MessageImpl<ErrorResponse> {
   public:
    explicit ErrorResponse(std::error_code code, const Str& message);
    explicit ErrorResponse(const Error& error);

    static Result<std::unique_ptr<ErrorResponse>> create(const kc::Json& body);
    static std::unique_ptr<ErrorResponse> create(const Error& error);

    const Error& error() const { return m_error; }

   private:
    kc::Json serializeBody() const override;
    Error m_error;
};

}  // namespace anchor::proto
