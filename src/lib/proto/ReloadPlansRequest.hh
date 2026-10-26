#pragma once

#include "Message.hh"

namespace anchor::proto {

class ReloadPlansRequest : public MessageImpl<ReloadPlansRequest> {
   public:
    ReloadPlansRequest();

    static Result<std::unique_ptr<ReloadPlansRequest>> create(const Json& body);

   private:
    Json serializeBody() const override;
};

}  // namespace anchor::proto
