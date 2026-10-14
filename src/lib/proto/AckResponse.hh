#pragma once

#include "Message.hh"

namespace anchor::proto {

class AckResponse : public MessageImpl<AckResponse> {
   public:
    AckResponse();
    static Result<std::unique_ptr<AckResponse>> create(const Json& body);

   private:
    Json serializeBody() const override;
};

}  // namespace anchor::proto
