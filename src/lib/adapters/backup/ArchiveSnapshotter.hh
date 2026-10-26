#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

class ArchiveSnapshotter : public Snapshotter {
   public:
    ArchiveSnapshotter(const FileSystem& fs, ArchiveType format);

    Result<SnapshotMetadata> snapshot(const SnapshotArgs& args) override;

   private:
    ArchiveType m_format;
    const FileSystem& m_fs;
};

}  // namespace anchor
