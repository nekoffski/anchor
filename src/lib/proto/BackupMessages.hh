#pragma once

#include <span>

#include "Message.hh"
#include "core/domain/Backup.hh"

namespace anchor::proto {

class TriggerBackupRequest : public MessageImpl<TriggerBackupRequest> {
   public:
    explicit TriggerBackupRequest(const Str& name);

    static Result<std::unique_ptr<TriggerBackupRequest>> create(
        const kc::Json& body
    );
    const Str& backupName() const { return m_name; }

   private:
    kc::Json serializeBody() const override;
    Str m_name;
};

class GetBackupsRequest : public MessageImpl<GetBackupsRequest> {
   public:
    explicit GetBackupsRequest();

    static Result<std::unique_ptr<GetBackupsRequest>> create(
        const kc::Json& body
    );

   private:
    kc::Json serializeBody() const override;
};

class GetBackupsResponse : public MessageImpl<GetBackupsResponse> {
   public:
    explicit GetBackupsResponse(std::span<const Backup>);
    explicit GetBackupsResponse();

    static Result<std::unique_ptr<GetBackupsResponse>> create(
        const kc::Json& body
    );

    const std::vector<Backup>& backups() const { return m_backups; }

   private:
    kc::Json serializeBody() const override;

    std::vector<Backup> m_backups;
};

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
