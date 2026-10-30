#pragma once

#include <kcore/Concepts.hh>

#include "core/Core.hh"
#include "core/Error.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

class ArchiveSnapshotter : public Snapshotter {
   public:
    ArchiveSnapshotter(const kc::FileSystem& fs, ArchiveType format);

    Result<SnapshotMetadata> snapshot(const SnapshotArgs& args) override;

   private:
    ArchiveType m_format;
    const kc::FileSystem& m_fs;
};

}  // namespace anchor
