#pragma once

#include "Message.hh"

namespace anchor::proto {

class TriggerBackupRequest : public MessageImpl<TriggerBackupRequest> {
   public:
    explicit TriggerBackupRequest(const Str& name);

    static Result<std::unique_ptr<TriggerBackupRequest>> create(
        const Json& body
    );
    const Str& backupName() const { return m_name; }

   private:
    Json serializeBody() const override;
    Str m_name;
};

}  // namespace anchor::proto
