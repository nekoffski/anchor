#pragma once

#include "base/Error.hh"
#include "base/FileSystem.hh"
#include "core/domain/Snapshot.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

class OnDiskSnapshotStore : public SnapshotStore {
   public:
    explicit OnDiskSnapshotStore(const FileSystem& fs);

    Result<SnapshotMetadata> put(
        const SnapshotMetadata& metadata, const Location& destination
    ) override;

   private:
    const FileSystem& m_fs;
};

}  // namespace anchor
