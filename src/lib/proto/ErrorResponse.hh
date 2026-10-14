#pragma once

#include "Message.hh"

namespace anchor::proto {

class ErrorResponse : public MessageImpl<ErrorResponse> {
   public:
    explicit ErrorResponse(ErrorCode code, const Str& message);
    explicit ErrorResponse(const Error& error);

    static Result<std::unique_ptr<ErrorResponse>> create(const Json& body);
    const Error& error() const { return m_error; }

   private:
    Json serializeBody() const override;
    Error m_error;
};

}  // namespace anchor::proto
