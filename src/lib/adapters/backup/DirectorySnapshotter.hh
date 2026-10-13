#pragma once

#include "base/FileSystem.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

class DirectorySnapshotter : public Snapshotter {
   public:
    explicit DirectorySnapshotter(const FileSystem& fs);

    Result<SnapshotMetadata> snapshot(
        const Str& name, std::span<const Filter> filters, const Path& workspace
    ) override;

   private:
    Str newSnapshotName(const Str& backupName);

    const FileSystem& m_fs;
};

}  // namespace anchor
