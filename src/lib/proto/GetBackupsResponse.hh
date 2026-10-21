#pragma once

#include <span>

#include "Message.hh"
#include "core/domain/Backup.hh"

namespace anchor::proto {

class GetBackupsResponse : public MessageImpl<GetBackupsResponse> {
   public:
    explicit GetBackupsResponse(std::span<const Backup>);
    explicit GetBackupsResponse();

    static Result<std::unique_ptr<GetBackupsResponse>> create(const Json& body);

    const std::vector<Backup>& backups() const { return m_backups; }

   private:
    Json serializeBody() const override;

    std::vector<Backup> m_backups;
};

}  // namespace anchor::proto
