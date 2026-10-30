#pragma once

#include "Message.hh"
#include "core/Error.hh"

namespace anchor::proto {

class GetBackupsRequest : public MessageImpl<GetBackupsRequest> {
   public:
    explicit GetBackupsRequest();

    static Result<std::unique_ptr<GetBackupsRequest>> create(
        const kc::Json& body
    );

   private:
    kc::Json serializeBody() const override;
};

}  // namespace anchor::proto
