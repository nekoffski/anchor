#pragma once

#include "Message.hh"
#include "core/Error.hh"

namespace anchor::proto {

class ReloadPlansRequest : public MessageImpl<ReloadPlansRequest> {
   public:
    ReloadPlansRequest();

    static Result<std::unique_ptr<ReloadPlansRequest>> create(
        const kc::Json& body
    );

   private:
    kc::Json serializeBody() const override;
};

}  // namespace anchor::proto
