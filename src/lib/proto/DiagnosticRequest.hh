#pragma once

#include "Message.hh"

namespace anchor::proto {

class DiagnosticRequest : public MessageImpl<DiagnosticRequest> {
   public:
    explicit DiagnosticRequest(const Str& details);

    static Result<std::unique_ptr<DiagnosticRequest>> create(const Json& body);
    const Str& details() const { return m_details; }

   private:
    Json serializeBody() const override;
    Str m_details;
};

}  // namespace anchor::proto
