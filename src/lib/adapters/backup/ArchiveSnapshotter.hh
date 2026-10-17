#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "core/ports/Snapshot.hh"

namespace anchor {

class ZipSnapshotter : public Snapshotter {
   public:
    explicit ZipSnapshotter(const FileSystem& fs);

    Result<SnapshotMetadata> snapshot(const SnapshotArgs& args) override;

   private:
    const FileSystem& m_fs;
};

class TgzSnapshotter : public Snapshotter {
   public:
   private:
};

}  // namespace anchor
