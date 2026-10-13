#pragma once

#include "base/FileSystem.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

class OnDiskSnapshotStore : public SnapshotStore {
   public:
    explicit OnDiskSnapshotStore(const FileSystem& fs);

   private:
    const FileSystem& m_fs;
};

}  // namespace anchor
