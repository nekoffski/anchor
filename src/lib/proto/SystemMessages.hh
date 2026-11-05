#pragma once

#include "Message.hh"

namespace anchor::proto {

class AckResponse : public MessageImpl<AckResponse> {
   public:
    AckResponse();
    static Result<std::unique_ptr<AckResponse>> create(const kc::Json& body);

   private:
    kc::Json serializeBody() const override;
};

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

class DiagnosticRequest : public MessageImpl<DiagnosticRequest> {
   public:
    explicit DiagnosticRequest(const Str& details);

    static Result<std::unique_ptr<DiagnosticRequest>> create(
        const kc::Json& body
    );
    const Str& details() const { return m_details; }

   private:
    kc::Json serializeBody() const override;
    Str m_details;
};

}  // namespace anchor::proto
