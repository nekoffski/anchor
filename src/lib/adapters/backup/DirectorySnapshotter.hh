#pragma once

#include <kcore/io/FileSystem.hh>

#include "core/Core.hh"
#include "core/Error.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

class DirectorySnapshotter : public Snapshotter {
   public:
    explicit DirectorySnapshotter(const kc::FileSystem& fs);

    Result<SnapshotMetadata> snapshot(const SnapshotArgs& args) override;

   private:
    const kc::FileSystem& m_fs;
};

}  // namespace anchor
