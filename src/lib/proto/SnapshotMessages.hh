#pragma once

#include "Message.hh"
#include "core/domain/Snapshot.hh"

namespace anchor::proto {

class GetSnapshotsRequest : public MessageImpl<GetSnapshotsRequest> {
   public:
    explicit GetSnapshotsRequest(const Str& backupName);

    static Result<std::unique_ptr<GetSnapshotsRequest>> create(
        const kc::Json& body
    );

    const Str& backupName() const { return m_backupName; }

   private:
    kc::Json serializeBody() const override;
    Str m_backupName;
};

class GetSnapshotRequest : public MessageImpl<GetSnapshotRequest> {
   public:
    GetSnapshotRequest(const Str& backupName, const Uuid& uuid);

    static Result<std::unique_ptr<GetSnapshotRequest>> create(
        const kc::Json& body
    );

    const Str& backupName() const { return m_backupName; }

    const Uuid& uuid() const { return m_uuid; }

   private:
    kc::Json serializeBody() const override;
    Str m_backupName;
    Uuid m_uuid;
};

class RemoveSnapshotRequest : public MessageImpl<RemoveSnapshotRequest> {
   public:
    RemoveSnapshotRequest(const Str& backupName, const Uuid& uuid);

    static Result<std::unique_ptr<RemoveSnapshotRequest>> create(
        const kc::Json& body
    );

    const Str& backupName() const { return m_backupName; }

    const Uuid& uuid() const { return m_uuid; }

   private:
    kc::Json serializeBody() const override;
    Str m_backupName;
    Uuid m_uuid;
};

class GetSnapshotsResponse : public MessageImpl<GetSnapshotsResponse> {
   public:
    GetSnapshotsResponse();

    explicit GetSnapshotsResponse(std::span<const SnapshotSummary> snapshots);

    const std::vector<SnapshotSummary>& snapshots() const {
        return m_snapshots;
    }

    static Result<std::unique_ptr<GetSnapshotsResponse>> create(
        const kc::Json& body
    );

   private:
    kc::Json serializeBody() const override;
    std::vector<SnapshotSummary> m_snapshots;
};

class GetSnapshotResponse : public MessageImpl<GetSnapshotResponse> {
   public:
    GetSnapshotResponse();

    static Result<std::unique_ptr<GetSnapshotResponse>> create(
        const kc::Json& body
    );

   private:
    kc::Json serializeBody() const override;
};

}  // namespace anchor::proto
