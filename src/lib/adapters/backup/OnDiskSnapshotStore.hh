#pragma once

#include <mutex>

#include "core/ports/Snapshot.hh"

namespace anchor {

class OnDiskSnapshotStore : public SnapshotStore {
   public:
    explicit OnDiskSnapshotStore(const kc::FileSystem& fs);

    Result<SnapshotMetadata> put(
        const SnapshotMetadata& metadata, const Location& destination
    ) override;
    Result<std::vector<SnapshotSummary>> list(
        const Location& destination, const Str& backupName
    ) const override;
    Result<void> remove(
        const Location& destination, const Str& backupName, const Uuid& uuid
    ) override;

   private:
    Result<void> publishUpload(
        const kc::Path& root, const kc::Path& source,
        const SnapshotSummary& snapshot, const Str& manifestContents
    );

    const kc::FileSystem& m_fs;
    mutable std::mutex m_mutex;
};

}  // namespace anchor
