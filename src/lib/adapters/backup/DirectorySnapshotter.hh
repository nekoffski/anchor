#pragma once

#include "base/FileSystem.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

class DirectorySnapshotter : public Snapshotter {
   public:
    explicit DirectorySnapshotter(const FileSystem& fs);

    Result<SnapshotMetadata> snapshot(const SnapshotArgs& args) override;

   private:
    Str newSnapshotName(const Str& backupName);

    Result<void> traverse(
        const Path& source, const Path& destination,
        const SourceTree::Nodes& nodes
    );

    const FileSystem& m_fs;
};

}  // namespace anchor
