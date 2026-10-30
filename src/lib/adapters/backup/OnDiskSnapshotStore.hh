#pragma once

#include <kcore/io/FileSystem.hh>

#include "core/Error.hh"
#include "core/domain/Snapshot.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

class OnDiskSnapshotStore : public SnapshotStore {
   public:
    explicit OnDiskSnapshotStore(const kc::FileSystem& fs);

    Result<SnapshotMetadata> put(
        const SnapshotMetadata& metadata, const Location& destination
    ) override;

   private:
    const kc::FileSystem& m_fs;
};

}  // namespace anchor
