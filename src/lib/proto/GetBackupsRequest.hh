#pragma once

#include "Message.hh"

namespace anchor::proto {

class GetBackupsRequest : public MessageImpl<GetBackupsRequest> {
   public:
    explicit GetBackupsRequest();

    static Result<std::unique_ptr<GetBackupsRequest>> create(const Json& body);

   private:
    Json serializeBody() const override;
};

}  // namespace anchor::proto
