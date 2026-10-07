#pragma once

#include "base/FileSystem.hh"
#include "core/ports/Snapshotter.hh"

namespace anchor {

class DirectorySnapshotter : public Snapshotter {
   public:
    explicit DirectorySnapshotter(const FileSystem& fs) : m_fs(fs) {}

    Result<SnapshotMetadata> snapshot(
        std::span<const Filter> filters, const Path& workspace
    ) override {
        for (const auto& filter : filters) {
            // Process each filter
        }
    }

   private:
    const FileSystem& m_fs;
};

}  // namespace anchor
