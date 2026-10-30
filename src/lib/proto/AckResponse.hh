#pragma once

#include "Message.hh"
#include "core/Error.hh"

namespace anchor::proto {

class AckResponse : public MessageImpl<AckResponse> {
   public:
    AckResponse();
    static Result<std::unique_ptr<AckResponse>> create(const kc::Json& body);

   private:
    kc::Json serializeBody() const override;
};

}  // namespace anchor::proto
