#pragma once

#include "base/FileSystem.hh"
#include "core/ports/SnapshotStore.hh"

namespace anchor {

class LocalSnapshotStore : public SnapshotStore {
   public:
    explicit LocalSnapshotStore(const FileSystem& fs) : m_fs(fs) {}

   private:
    const FileSystem& m_fs;
};

}  // namespace anchor
